// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROCESS_RANK_POLICY_ANDROID_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROCESS_RANK_POLICY_ANDROID_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/android/child_process_importance.h"

namespace performance_manager::policies {

// This class is responsible to set content::ChildProcessImportance to the
// WebContents on a page basis.
//
// This is Chrome on Android specific policy because:
//
// * Android is the only platform that Chrome can't detect memory pressure and
//   ends up relying on LMKD to kill renderer processes on memory pressure.
//   Chrome on Android never kill any renderer process on memory pressure while
//   UrgentPageDiscardingPolicy discards tabs on critical memory pressure on
//   other platforms than Android.
// * WebView on Android directly updates importance via
//   WebContentsAndroid::SetImportance() from
//   WebView.setRendererPriorityPolicy() API and does not enable Performance
//   Manager.
//
// ProcessRankPolicyAndroid keep the memory priority in sync with LMKD by
// content::ChildProcessImportance on any page status change unlike
// UrgentPageDiscardingPolicy calculates memory priority of all pages at once
// only on critical memory pressure.
class ProcessRankPolicyAndroid : public GraphOwned,
                                 public PageNodeObserver,
                                 public PageLiveStateObserverDefaultImpl {
 public:
  ProcessRankPolicyAndroid();
  ~ProcessRankPolicyAndroid() override;
  ProcessRankPolicyAndroid(const ProcessRankPolicyAndroid& other) = delete;
  ProcessRankPolicyAndroid& operator=(const ProcessRankPolicyAndroid&) = delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsFocusedChanged(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;

  // PageLiveStateObserver implementation:
  void OnIsActiveTabChanged(const PageNode* page_node) override;

 private:
  void UpdateProcessRank(const PageNode* page_node);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROCESS_RANK_POLICY_ANDROID_H_
