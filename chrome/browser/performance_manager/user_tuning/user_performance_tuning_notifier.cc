// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"

namespace performance_manager::user_tuning {

UserPerformanceTuningNotifier::UserPerformanceTuningNotifier(
    std::unique_ptr<Receiver> receiver,
    int tab_count_threshold)
    : receiver_(std::move(receiver)),
      tab_count_threshold_(tab_count_threshold) {}

UserPerformanceTuningNotifier::~UserPerformanceTuningNotifier() = default;

void UserPerformanceTuningNotifier::OnPassedToGraph(Graph* graph) {
  CHECK(graph->GetAllPageNodes().empty());
  graph->AddPageNodeObserver(this);
}

void UserPerformanceTuningNotifier::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
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
