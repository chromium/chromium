// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"

#include <utility>
#include <vector>

#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager::user_tuning {

const int UserPerformanceTuningNotifier::kTabCountThresholdForPromo = 10;
const int UserPerformanceTuningNotifier::kMemoryPercentThresholdForPromo = 70;

UserPerformanceTuningNotifier::UserPerformanceTuningNotifier(
    std::unique_ptr<Receiver> receiver,
    uint64_t resident_set_threshold_kb,
    int tab_count_threshold)
    : receiver_(std::move(receiver)),
      resident_set_threshold_kb_(resident_set_threshold_kb),
      tab_count_threshold_(tab_count_threshold) {}

UserPerformanceTuningNotifier::~UserPerformanceTuningNotifier() = default;

void UserPerformanceTuningNotifier::OnPassedToGraph(Graph* graph) {
  CHECK(graph->GetAllPageNodes().empty());
  graph_ = graph;
  graph->AddPageNodeObserver(this);

  metrics_interest_token_ = performance_manager::ProcessMetricsDecorator::
      RegisterInterestForProcessMetrics(graph);
  graph->AddSystemNodeObserver(this);
}

void UserPerformanceTuningNotifier::OnTakenFromGraph(Graph* graph) {
  graph->RemoveSystemNodeObserver(this);
  metrics_interest_token_.reset();

  graph->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void UserPerformanceTuningNotifier::OnPageNodeAdded(const PageNode* page_node) {
  MaybeAddTabAndNotify(page_node);
}

void UserPerformanceTuningNotifier::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  if (page_node->GetType() == PageType::kTab) {
    DCHECK_GT(tab_count_, 0);
    --tab_count_;
  }
}

void UserPerformanceTuningNotifier::OnTypeChanged(const PageNode* page_node,
                                                  PageType previous_type) {
  if (previous_type == PageType::kTab) {
    DCHECK_GT(tab_count_, 0);
    DCHECK_NE(page_node->GetType(), PageType::kTab);
    --tab_count_;
  } else {
    MaybeAddTabAndNotify(page_node);
  }
}

void UserPerformanceTuningNotifier::OnProcessMemoryMetricsAvailable(
    const SystemNode* system_node) {
  uint64_t total_rss = 0;
  for (const ProcessNode* process_node : graph_->GetAllProcessNodes()) {
    total_rss += process_node->GetResidentSetKb();
  }

  // Only notify when the threshold is crossed, not if an update keeps the total
  // RSS above the threshold.
  if (total_rss >= resident_set_threshold_kb_ &&
      previous_total_rss_ < resident_set_threshold_kb_) {
    receiver_->NotifyMemoryThresholdReached();
  }

  previous_total_rss_ = total_rss;

  ProxyAndPmfKbVector proxies_and_pmf;
  std::vector<const PageNode*> all_page_nodes = graph_->GetAllPageNodes();
  proxies_and_pmf.reserve(all_page_nodes.size());

  for (auto* page_node : all_page_nodes) {
    proxies_and_pmf.emplace_back(page_node->GetContentsProxy(),
                                 page_node->EstimatePrivateFootprintSize());
  }

  receiver_->NotifyMemoryMetricsRefreshed(std::move(proxies_and_pmf));
}

void UserPerformanceTuningNotifier::MaybeAddTabAndNotify(
    const PageNode* page_node) {
  if (page_node->GetType() == PageType::kTab) {
    ++tab_count_;

    // The notification is only sent when the threshold is crossed, not every
    // time a tab is added above the threshold.
    if (tab_count_ == tab_count_threshold_) {
      receiver_->NotifyTabCountThresholdReached();
    }
  }
}

}  // namespace performance_manager::user_tuning
