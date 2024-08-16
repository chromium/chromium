// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_cluster_metrics.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_category_metrics.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#include <set>
#include <string>

namespace {

bool HasSetIntersection(const std::set<std::string>& cluster_category_ids,
                        const std::set<std::string>& category_ids) {
  std::vector<std::string> categories_intersection;
  std::set_intersection(cluster_category_ids.begin(),
                        cluster_category_ids.end(), category_ids.begin(),
                        category_ids.end(),
                        std::back_inserter(categories_intersection));

  return !categories_intersection.empty();
}

}  // namespace

HistoryClustersModuleRankingSignals::HistoryClustersModuleRankingSignals(
    const std::vector<CartDB::KeyAndValue>& active_carts,
    const base::flat_set<std::string>& category_boostlist,
    const history::Cluster& cluster,
    const HistoryClusterMetrics& metrics,
    const HistoryClustersCategoryMetrics& category_metrics)
    : duration_since_most_recent_visit(
          base::Time::Now() -
          cluster.GetMostRecentVisit().annotated_visit.visit_row.visit_time),
      belongs_to_boosted_category(
          category_boostlist.empty()
              ? false
              : history_clusters::IsClusterInCategories(cluster,
                                                        category_boostlist)),
      num_total_visits(cluster.visits.size()),
      num_times_seen_last_24h(metrics.num_times_seen),
      num_times_used_last_24h(metrics.num_times_used) {
  base::flat_set<std::string> hosts;
  base::flat_set<std::string> cart_tlds;
  for (const auto& visit : cluster.visits) {
    if (visit.annotated_visit.visit_row.is_known_to_sync &&
        visit.annotated_visit.content_annotations.has_url_keyed_image) {
      num_visits_with_image++;
    }

    hosts.insert(visit.normalized_url.host());

    if (!active_carts.empty()) {
      std::string visit_tld =
          net::registry_controlled_domains::GetDomainAndRegistry(
              visit.normalized_url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    }
  }

  num_unique_hosts = hosts.size();
  num_abandoned_carts = cart_tlds.size();
  const auto cluster_categories =
      history_clusters::GetClusterCategoryIds(cluster);
  num_associated_categories = cluster_categories.size();
  belongs_to_most_seen_category = HasSetIntersection(
      cluster_categories, category_metrics.most_frequently_seen_category_ids);
  belongs_to_most_used_category = HasSetIntersection(
      cluster_categories, category_metrics.most_frequently_used_category_ids);
  most_frequent_category_seen_count_last_24h =
      category_metrics.most_frequent_seen_category_for_period_count;
  most_frequent_category_used_count_last_24h =
      category_metrics.most_frequent_used_category_for_period_count;
}

HistoryClustersModuleRankingSignals::HistoryClustersModuleRankingSignals() =
    default;
HistoryClustersModuleRankingSignals::~HistoryClustersModuleRankingSignals() =
    default;
HistoryClustersModuleRankingSignals::HistoryClustersModuleRankingSignals(
    const HistoryClustersModuleRankingSignals&) = default;

void HistoryClustersModuleRankingSignals::PopulateUkmEntry(
    ukm::builders::NewTabPage_HistoryClusters* ukm_entry_builder) const {
  ukm_entry_builder->SetMinutesSinceMostRecentVisit(
      ukm::GetExponentialBucketMin(duration_since_most_recent_visit.InMinutes(),
                                   /*bucket_spacing=*/1.3));
  ukm_entry_builder->SetBelongsToBoostedCategory(belongs_to_boosted_category);
  ukm_entry_builder->SetNumVisitsWithImage(num_visits_with_image);
  ukm_entry_builder->SetNumTotalVisits(num_total_visits);
  ukm_entry_builder->SetNumUniqueHosts(num_unique_hosts);
  ukm_entry_builder->SetNumAbandonedCarts(num_abandoned_carts);
  ukm_entry_builder->SetNumTimesSeenLast24h(num_times_seen_last_24h);
  ukm_entry_builder->SetNumTimesUsedLast24h(num_times_used_last_24h);
  ukm_entry_builder->SetNumAssociatedCategories(num_associated_categories);
  ukm_entry_builder->SetBelongsToMostSeenCategory(
      belongs_to_most_seen_category);
  ukm_entry_builder->SetBelongsToMostUsedCategory(
      belongs_to_most_used_category);
  ukm_entry_builder->SetMostFrequentSeenCategoryCount(
      most_frequent_category_seen_count_last_24h);
  ukm_entry_builder->SetMostFrequentUsedCategoryCount(
      most_frequent_category_used_count_last_24h);
}
