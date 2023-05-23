// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"

#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

HistoryClustersModuleRankingSignals::HistoryClustersModuleRankingSignals(
    const std::vector<CartDB::KeyAndValue>& active_carts,
    const base::flat_set<std::string>& category_boostlist,
    const history::Cluster& cluster)
    : duration_since_most_recent_visit(
          base::Time::Now() -
          cluster.GetMostRecentVisit().annotated_visit.visit_row.visit_time),
      belongs_to_boosted_category(
          category_boostlist.empty()
              ? false
              : history_clusters::IsClusterInCategories(cluster,
                                                        category_boostlist)),
      num_total_visits(cluster.visits.size()) {
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
      for (auto cart : active_carts) {
        if (CartProcessor::IsCartAssociatedWithVisitURL(cart,
                                                        visit.normalized_url)) {
          cart_tlds.insert(visit_tld);
        }
      }
    }
  }

  num_unique_hosts = hosts.size();
  num_abandoned_carts = cart_tlds.size();
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
}
