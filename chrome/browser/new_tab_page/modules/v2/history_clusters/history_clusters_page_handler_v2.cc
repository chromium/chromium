// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/history_clusters/history_clusters_page_handler_v2.h"

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/discount/discount.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/discount/discount_processor.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_metrics_logger.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/new_tab_page/modules/v2/history_clusters/history_clusters_v2.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_cluster_type_utils.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/search/ntp_features.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/strings/grit/components_strings.h"

namespace {

// The minimum number of visits to render a layout is 2 URL visits plus a SRP
// visit.
constexpr int kMinRequiredVisits = 3;

constexpr int kMinRequiredRelatedSearches = 2;

constexpr char kDismissReasonMetricName[] =
    "NewTabPage.HistoryClusters.DismissReason";

// This enum must match NTPHistoryClustersDismissReason in enums.xml. Do not
// reorder or remove items, and update kMaxValue when new items are added.
enum NTPHistoryClustersDismissReason {
  kNotInterested = 0,
  kDone = 1,
  kMaxValue = kDone,
};

// Returns a deduped list of category ids associated with a given cluster sorted
// descending by category weight and meeting a minimum weight requirement.
std::vector<std::string> GetClusterCategoryIdsDescendingSortedByWeight(
    const history::Cluster& cluster,
    int min_weight) {
  std::map<std::string, history::VisitContentModelAnnotations::Category>
      categories_map;

  for (const auto& visit : cluster.visits) {
    auto& visit_categories =
        visit.annotated_visit.content_annotations.model_annotations.categories;
    for (const auto& category : visit_categories) {
      if (category.weight < min_weight) {
        continue;
      }
      // If the visit category is already associated to the cluster, retain
      // the highest weight value.
      if (base::Contains(categories_map, category.id)) {
        categories_map[category.id].weight =
            std::max(categories_map[category.id].weight, category.weight);
      } else {
        categories_map[category.id] = category;
      }
    }
  }

  std::vector<history::VisitContentModelAnnotations::Category> categories;
  std::transform(categories_map.cbegin(), categories_map.cend(),
                 std::back_inserter(categories),
                 [](auto& category_id_value_pair) {
                   return category_id_value_pair.second;
                 });
  base::ranges::stable_sort(categories, [](const auto& c1, const auto& c2) {
    return c1.weight > c2.weight;
  });
  std::vector<std::string> category_ids;
  std::transform(categories.cbegin(), categories.cend(),
                 std::back_inserter(category_ids),
                 [](auto& category) { return category.id; });
  return category_ids;
}

}  // namespace

HistoryClustersPageHandlerV2::HistoryClustersPageHandlerV2(
    mojo::PendingReceiver<ntp::history_clusters_v2::mojom::PageHandler>
        pending_receiver,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      ranking_metrics_logger_(
          std::make_unique<HistoryClustersModuleRankingMetricsLogger>(
              web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId())),
      min_category_weight_to_record_(GetFieldTrialParamByFeatureAsInt(
          ntp_features::kNtpHistoryClustersModuleCategories,
          ntp_features::kNtpHistoryClustersModuleMinCategoryWeightToRecordParam,
          90)),
      max_categories_to_record_per_cluster_(GetFieldTrialParamByFeatureAsInt(
          ntp_features::kNtpHistoryClustersModuleCategories,
          ntp_features::kNtpHistoryClustersModuleMaxCategoriesToRecordParam,
          5)),
      receiver_(this, std::move(pending_receiver)) {
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpChromeCartInHistoryClusterModule)) {
    cart_processor_ = std::make_unique<CartProcessor>(
        CartServiceFactory::GetForProfile(profile_));
  }

    discount_processor_ = std::make_unique<DiscountProcessor>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(profile_));
}

HistoryClustersPageHandlerV2::~HistoryClustersPageHandlerV2() {
  receiver_.reset();

  ranking_metrics_logger_->RecordUkm(/*record_in_cluster_id_order=*/false);
}

void HistoryClustersPageHandlerV2::CallbackWithClusterData(
    GetClustersCallback callback,
    std::vector<history::Cluster> clusters,
    base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
        ranking_signals) {
  if (clusters.empty()) {
    std::move(callback).Run({});
    return;
  }

  ranking_metrics_logger_->AddSignals(std::move(ranking_signals));

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  for (const auto& cluster : clusters) {
    clusters_mojom.push_back(history_clusters::ClusterToMojom(
        TemplateURLServiceFactory::GetForProfile(profile_), cluster));

    auto category_ids = GetClusterCategoryIdsDescendingSortedByWeight(
        cluster, min_category_weight_to_record_);
    base::UmaHistogramCounts100(
        "NewTabPage.HistoryClusters.CategoryCountForMinWeightThreshold",
        category_ids.size());
    if (category_ids.size() > max_categories_to_record_per_cluster_) {
      category_ids.resize(max_categories_to_record_per_cluster_);
    }
    cluster_categories_[cluster.cluster_id] = std::move(category_ids);
  }

  std::move(callback).Run(std::move(clusters_mojom));
}

void HistoryClustersPageHandlerV2::GetClusters(GetClustersCallback callback) {
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpHistoryClustersModule,
      ntp_features::kNtpHistoryClustersModuleDataParam);

  if (!fake_data_param.empty()) {
    const std::vector<std::string> kFakeDataParams = base::SplitString(
        fake_data_param, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (kFakeDataParams.size() != 3) {
      LOG(ERROR) << "Invalid history clusters fake data selection parameter "
                    "format.";
      std::move(callback).Run({});
      return;
    }

    int num_clusters;
    int num_visits;
    int num_images;
    if (!base::StringToInt(kFakeDataParams.at(0), &num_clusters) ||
        !base::StringToInt(kFakeDataParams.at(1), &num_visits) ||
        !base::StringToInt(kFakeDataParams.at(2), &num_images) ||
        num_visits < num_images) {
      std::move(callback).Run({});
      return;
    }

    std::vector<history::Cluster> clusters;
    for (int i = 0; i < num_clusters; i++) {
      clusters.push_back(GenerateSampleCluster(i, num_visits, num_images));
    }
    CallbackWithClusterData(std::move(callback), std::move(clusters), {});
    return;
  }

  auto* history_clusters_module_service =
      HistoryClustersModuleServiceFactory::GetForProfile(profile_);
  if (!history_clusters_module_service) {
    std::move(callback).Run({});
    return;
  }
  history_clusters::QueryClustersFilterParams filter_params =
      CreateFilterParamsFromFeatureFlags(kMinRequiredVisits,
                                         kMinRequiredRelatedSearches);
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleTextOnly)) {
    filter_params.min_visits_with_images = 0;
  }
  history_clusters_module_service->GetClusters(
      filter_params, static_cast<size_t>(kMinRequiredRelatedSearches),
      base::BindOnce(&HistoryClustersPageHandlerV2::CallbackWithClusterData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HistoryClustersPageHandlerV2::GetCartForCluster(
    history_clusters::mojom::ClusterPtr cluster,
    GetCartForClusterCallback callback) {
  if (!base::FeatureList::IsEnabled(
          ntp_features::kNtpChromeCartInHistoryClusterModule)) {
    std::move(callback).Run(nullptr);
    return;
  }
  DCHECK(cart_processor_);
  cart_processor_->GetCartForCluster(std::move(cluster), std::move(callback));
}

void HistoryClustersPageHandlerV2::GetDiscountsForCluster(
    history_clusters::mojom::ClusterPtr cluster,
    GetDiscountsForClusterCallback callback) {
  DCHECK(discount_processor_);
  discount_processor_->GetDiscountsForCluster(std::move(cluster),
                                              std::move(callback));
}

void HistoryClustersPageHandlerV2::ShowJourneysSidePanel(
    const std::string& query) {
  // TODO(crbug.com/1341399): Revisit integration with the side panel once the
  // referenced bug is resolved.
  auto* history_clusters_tab_helper =
      side_panel::HistoryClustersTabHelper::FromWebContents(web_contents_);
  history_clusters_tab_helper->ShowJourneysSidePanel(query);
}

void HistoryClustersPageHandlerV2::RecordClick(int64_t cluster_id) {
  ranking_metrics_logger_->SetClicked(cluster_id);

  std::map<std::string, uint64_t> category_counts;
  std::transform(
      cluster_categories_[cluster_id].cbegin(),
      cluster_categories_[cluster_id].cend(),
      std::inserter(category_counts, category_counts.end()),
      [](auto& category_id) { return std::make_pair(category_id, 1u); });
  segmentation_platform::DatabaseClient::StructuredEvent cluster_ids_event = {
      kHistoryClusterUsedEventName, {{base::NumberToString(cluster_id), 1}}};
  segmentation_platform::DatabaseClient::StructuredEvent
      cluster_categories_event = {kHistoryClustersUsedCategoriesEventName,
                                  std::move(category_counts)};
  base::UmaHistogramBoolean(
      "NewTabPage.HistoryClusters.SegmentationPlatformClientReadyAtUsed",
      MaybeRecordEvents({&cluster_ids_event, &cluster_categories_event}));
}

void HistoryClustersPageHandlerV2::RecordDisabled(int64_t cluster_id) {
  ranking_metrics_logger_->SetDisabled(cluster_id);
}

void HistoryClustersPageHandlerV2::RecordLayoutTypeShown(
    ntp::history_clusters::mojom::LayoutType layout_type,
    int64_t cluster_id) {
  ranking_metrics_logger_->SetLayoutTypeShown(layout_type, cluster_id);

  std::map<std::string, uint64_t> cluster_id_counts = {
      {base::NumberToString(cluster_id), 1}};
  std::map<std::string, uint64_t> category_counts;
  std::transform(
      cluster_categories_[cluster_id].cbegin(),
      cluster_categories_[cluster_id].cend(),
      std::inserter(category_counts, category_counts.end()),
      [](auto& category_id) { return std::make_pair(category_id, 1u); });
  segmentation_platform::DatabaseClient::StructuredEvent cluster_ids_event = {
      kHistoryClusterSeenEventName, std::move(cluster_id_counts)};
  segmentation_platform::DatabaseClient::StructuredEvent
      cluster_categories_event = {kHistoryClustersSeenCategoriesEventName,
                                  std::move(category_counts)};
  base::UmaHistogramBoolean(
      "NewTabPage.HistoryClusters.SegmentationPlatformClientReadyAtSeen",
      MaybeRecordEvents({&cluster_ids_event, &cluster_categories_event}));
}

void HistoryClustersPageHandlerV2::UpdateClusterVisitsInteractionState(
    int64_t cluster_id,
    const std::vector<history_clusters::mojom::URLVisitPtr> visits,
    const history_clusters::mojom::InteractionState state) {
  if (visits.empty()) {
    return;
  }

  std::vector<history::VisitID> visit_ids;
  base::ranges::transform(
      visits, std::back_inserter(visit_ids),
      [](const auto& url_visit_ptr) { return url_visit_ptr->visit_id; });

  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);

  history_service->UpdateVisitsInteractionState(
      visit_ids, static_cast<history::ClusterVisit::InteractionState>(state),
      base::BindOnce([]() {}), &update_visits_task_tracker_);

  switch (state) {
    case history_clusters::mojom::InteractionState::kHidden:
      base::UmaHistogramEnumeration(
          kDismissReasonMetricName,
          NTPHistoryClustersDismissReason::kNotInterested);
      ranking_metrics_logger_->SetDismissed(cluster_id);
      break;
    case history_clusters::mojom::InteractionState::kDone:
      base::UmaHistogramEnumeration(kDismissReasonMetricName,
                                    NTPHistoryClustersDismissReason::kDone);
      ranking_metrics_logger_->SetDismissed(cluster_id);
      ranking_metrics_logger_->SetMarkedAsDone(cluster_id);
      break;
    case history_clusters::mojom::InteractionState::kDefault:
      // Can happen when performing an 'Undo' action on the client,
      // which restores a cluster to the Default state.
      ranking_metrics_logger_->SetDismissed(cluster_id, false);
      ranking_metrics_logger_->SetMarkedAsDone(cluster_id, false);
      break;
  }
}

bool HistoryClustersPageHandlerV2::MaybeRecordEvents(
    const std::vector<segmentation_platform::DatabaseClient::StructuredEvent*>&
        events) {
  segmentation_platform::SegmentationPlatformService* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile_);
  segmentation_platform::DatabaseClient* client = service->GetDatabaseClient();
  // The client will be null until `IsPlatformInitialized()` is true.
  if (client) {
    for (const auto* event : events) {
      client->AddEvent(*event);
    }
  }

  return client != nullptr;
}
