// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_NOTIFIER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_NOTIFIER_H_

#include <memory>

#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager::user_tuning {

// This helper lives on the Performance Manager sequence to observe changes to
// the graph and notify the UserPerformanceTuningManager when certain thresholds
// are met.
class UserPerformanceTuningNotifier : public performance_manager::GraphOwned,
                                      public PageNode::ObserverDefaultImpl,
                                      public SystemNode::ObserverDefaultImpl {
 public:
  // The tab count and memory % that, when reached, trigger an opt-in bubble for
  // memory saver.
  static const int kTabCountThresholdForPromo;
  static const int kMemoryPercentThresholdForPromo;

  // The instance of this delegate will have its different functions invoked on
  // the Performance Manager sequence by the
  // `UserPerformanceTuningNotifier` owning it.
  class Receiver {
   public:
    virtual ~Receiver() = default;

    // Called when the current tab count reaches the threshold specified by
    // `tab_count_threshold`.
    virtual void NotifyTabCountThresholdReached() = 0;

    // Called when the current total resident set size of all processes exceeds
    // `resident_set_threshold_kb`.
    virtual void NotifyMemoryThresholdReached() = 0;
  };

  UserPerformanceTuningNotifier(std::unique_ptr<Receiver> delegate,
                                uint64_t resident_set_threshold_kb,
                                int tab_count_threshold);
  ~UserPerformanceTuningNotifier() override;

  // performance_manager::GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNode::ObserverDefaultImpl:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override;

  // SystemNode::ObserverDefaultImpl:
  void OnProcessMemoryMetricsAvailable(const SystemNode* system_node) override;

 private:
  void MaybeAddTabAndNotify(const PageNode* page_node);

  std::unique_ptr<Receiver> receiver_;

  std::unique_ptr<
      performance_manager::ProcessMetricsDecorator::ScopedMetricsInterestToken>
      metrics_interest_token_;
  const uint64_t resident_set_threshold_kb_ = 0;
  uint64_t previous_total_rss_ = 0;

  const int tab_count_threshold_ = 0;
  int tab_count_ = 0;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_NOTIFIER_H_
