// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_NOTIFIER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_NOTIFIER_H_

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::user_tuning {

// This helper lives on the Performance Manager sequence to observe changes to
// the graph and notify the UserPerformanceTuningManager when certain thresholds
// are met.
class UserPerformanceTuningNotifier : public performance_manager::GraphOwned,
                                      public PageNode::ObserverDefaultImpl {
 public:
  // The instance of this delegate will have its different functions invoked on
  // the Performance Manager sequence by the
  // `UserPerformanceTuningNotifier` owning it.
  class Receiver {
   public:
    virtual ~Receiver() = default;

    // Called when the current tab count reaches the threshold specified by
    // `tab_count_threshold`.
    virtual void NotifyTabCountThresholdReached() = 0;
  };

  UserPerformanceTuningNotifier(std::unique_ptr<Receiver> delegate,
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

 private:
  void MaybeAddTabAndNotify(const PageNode* page_node);

  std::unique_ptr<Receiver> receiver_;

  const int tab_count_threshold_ = 0;
  int tab_count_ = 0;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_USER_PERFORMANCE_TUNING_NOTIFIER_H_
