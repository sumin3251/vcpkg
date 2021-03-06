#pragma once
#include "PackageSpec.h"
#include "StatusParagraphs.h"
#include "VcpkgPaths.h"
#include "vcpkg_Graphs.h"
#include "vcpkg_optional.h"
#include <vector>

namespace vcpkg::Dependencies
{
    enum class RequestType
    {
        UNKNOWN,
        USER_REQUESTED,
        AUTO_SELECTED
    };

    std::string to_output_string(RequestType request_type, const CStringView s);

    struct AnyParagraph
    {
        std::vector<PackageSpec> dependencies(const Triplet& triplet) const;

        Optional<StatusParagraph> status_paragraph;
        Optional<BinaryParagraph> binary_paragraph;
        Optional<SourceParagraph> source_paragraph;
        Optional<const SourceControlFile*> source_control_file;
    };
}

namespace vcpkg::Dependencies
{
    struct FeatureSpec
    {
        PackageSpec spec;
        std::string feature_name;
    };

    struct FeatureNodeEdges
    {
        std::vector<FeatureSpec> remove_edges;
        std::vector<FeatureSpec> build_edges;
        bool plus = false;
    };
    std::vector<FeatureSpec> to_feature_specs(const std::vector<std::string>& depends, const Triplet& t);

    struct Cluster
    {
        std::vector<StatusParagraph> status_paragraphs;
        Optional<const SourceControlFile*> source_control_file;
        PackageSpec spec;
        std::unordered_map<std::string, FeatureNodeEdges> edges;
        std::unordered_set<std::string> to_install_features;
        std::unordered_set<std::string> original_features;
        bool will_remove = false;
        bool transient_uninstalled = true;
        Cluster() = default;

    private:
        Cluster(const Cluster&) = delete;
        Cluster& operator=(const Cluster&) = delete;
    };

    struct ClusterPtr
    {
        Cluster* ptr;
    };

    bool operator==(const ClusterPtr& l, const ClusterPtr& r);

    enum class InstallPlanType
    {
        UNKNOWN,
        BUILD_AND_INSTALL,
        INSTALL,
        ALREADY_INSTALLED
    };

    struct InstallPlanAction
    {
        static bool compare_by_name(const InstallPlanAction* left, const InstallPlanAction* right);

        InstallPlanAction();
        InstallPlanAction(const PackageSpec& spec, const AnyParagraph& any_paragraph, const RequestType& request_type);
        InstallPlanAction(const PackageSpec& spec,
                          const SourceControlFile& any_paragraph,
                          const std::unordered_set<std::string>& features,
                          const RequestType& request_type);
        InstallPlanAction(const InstallPlanAction&) = delete;
        InstallPlanAction(InstallPlanAction&&) = default;
        InstallPlanAction& operator=(const InstallPlanAction&) = delete;
        InstallPlanAction& operator=(InstallPlanAction&&) = default;

        PackageSpec spec;
        AnyParagraph any_paragraph;
        InstallPlanType plan_type;
        RequestType request_type;
        std::unordered_set<std::string> feature_list;
    };

    enum class RemovePlanType
    {
        UNKNOWN,
        NOT_INSTALLED,
        REMOVE
    };

    struct RemovePlanAction
    {
        static bool compare_by_name(const RemovePlanAction* left, const RemovePlanAction* right);

        RemovePlanAction();
        RemovePlanAction(const PackageSpec& spec, const RemovePlanType& plan_type, const RequestType& request_type);
        RemovePlanAction(const RemovePlanAction&) = delete;
        RemovePlanAction(RemovePlanAction&&) = default;
        RemovePlanAction& operator=(const RemovePlanAction&) = delete;
        RemovePlanAction& operator=(RemovePlanAction&&) = default;

        PackageSpec spec;
        RemovePlanType plan_type;
        RequestType request_type;
    };

    struct AnyAction
    {
        Optional<InstallPlanAction> install_plan;
        Optional<RemovePlanAction> remove_plan;
    };

    enum class ExportPlanType
    {
        UNKNOWN,
        PORT_AVAILABLE_BUT_NOT_BUILT,
        ALREADY_BUILT
    };

    struct ExportPlanAction
    {
        static bool compare_by_name(const ExportPlanAction* left, const ExportPlanAction* right);

        ExportPlanAction();
        ExportPlanAction(const PackageSpec& spec, const AnyParagraph& any_paragraph, const RequestType& request_type);
        ExportPlanAction(const ExportPlanAction&) = delete;
        ExportPlanAction(ExportPlanAction&&) = default;
        ExportPlanAction& operator=(const ExportPlanAction&) = delete;
        ExportPlanAction& operator=(ExportPlanAction&&) = default;

        PackageSpec spec;
        AnyParagraph any_paragraph;
        ExportPlanType plan_type;
        RequestType request_type;
    };

    __interface PortFileProvider { virtual const SourceControlFile& get_control_file(const PackageSpec& spec) const; };

    struct MapPortFile : PortFileProvider
    {
        const std::unordered_map<PackageSpec, SourceControlFile>& ports;
        explicit MapPortFile(const std::unordered_map<PackageSpec, SourceControlFile>& map);
        const SourceControlFile& get_control_file(const PackageSpec& spec) const override;
    };

    struct PathsPortFile : PortFileProvider
    {
        const VcpkgPaths& ports;
        mutable std::unordered_map<PackageSpec, SourceControlFile> cache;
        explicit PathsPortFile(const VcpkgPaths& paths);
        const SourceControlFile& get_control_file(const PackageSpec& spec) const override;

    private:
        PathsPortFile(const PathsPortFile&) = delete;
        PathsPortFile& operator=(const PathsPortFile&) = delete;
    };

    std::vector<InstallPlanAction> create_install_plan(const PortFileProvider& port_file_provider,
                                                       const std::vector<PackageSpec>& specs,
                                                       const StatusParagraphs& status_db);

    std::vector<RemovePlanAction> create_remove_plan(const std::vector<PackageSpec>& specs,
                                                     const StatusParagraphs& status_db);

    std::vector<ExportPlanAction> create_export_plan(const VcpkgPaths& paths,
                                                     const std::vector<PackageSpec>& specs,
                                                     const StatusParagraphs& status_db);
}

template<>
struct std::hash<vcpkg::Dependencies::ClusterPtr>
{
    size_t operator()(const vcpkg::Dependencies::ClusterPtr& value) const
    {
        return std::hash<vcpkg::PackageSpec>()(value.ptr->spec);
    }
};

namespace vcpkg::Dependencies
{
    struct GraphPlan
    {
        Graphs::Graph<ClusterPtr> remove_graph;
        Graphs::Graph<ClusterPtr> install_graph;
    };
    bool mark_plus(const std::string& feature,
                   Cluster& cluster,
                   std::unordered_map<PackageSpec, Cluster>& pkg_to_cluster,
                   GraphPlan& graph_plan);
    void mark_minus(Cluster& cluster, std::unordered_map<PackageSpec, Cluster>& pkg_to_cluster, GraphPlan& graph_plan);

    std::vector<AnyAction> create_feature_install_plan(const std::unordered_map<PackageSpec, SourceControlFile>& map,
                                                       const std::vector<FullPackageSpec>& specs,
                                                       const StatusParagraphs& status_db);
}
