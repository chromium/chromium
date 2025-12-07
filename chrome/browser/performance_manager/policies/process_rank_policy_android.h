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
                                 public PageLiveStateObserver {
 public:
  ProcessRankPolicyAndroid();
  explicit ProcessRankPolicyAndroid(bool is_perceptible_importance_supported);
  ~ProcessRankPolicyAndroid() override;
  ProcessRankPolicyAndroid(const ProcessRankPolicyAndroid& other) = delete;
  ProcessRankPolicyAndroid& operator=(const ProcessRankPolicyAndroid&) = delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override;
  void OnIsFocusedChanged(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;
  void OnHasPictureInPictureChanged(const PageNode* page_node) override;
  void OnMainFrameUrlChanged(const PageNode* page_node) override;
  // Change on GetContentsMimeType() is notified by
  // OnMainFrameDocumentChanged().
  void OnMainFrameDocumentChanged(const PageNode* page_node) override;
  void OnPageNotificationPermissionStatusChange(
      const PageNode* page_node,
      std::optional<blink::mojom::PermissionStatus> previous_status) override;
  void OnHadFormInteractionChanged(const PageNode* page_node) override;
  void OnHadUserEditsChanged(const PageNode* page_node) override;
  // TODO(crbug.com/410444953):
  // `DiscardEligibilityPolicy::IsPageOptedOutOfDiscarding()` depends on
  // `PageNode::GetMainFrameUrl()` and
  // `DiscardEligibilityPolicy::profiles_no_discard_patterns_`. We need observer
  // for `DiscardEligibilityPolicy::profiles_no_discard_patterns_` changes when
  // we enable memory saver mode on Android.

  // PageLiveStateObserver implementation:
  void OnIsActiveTabChanged(const PageNode* page_node) override;
  void OnIsAutoDiscardableChanged(const PageNode* page_node) override;
  void OnIsCapturingVideoChanged(const PageNode* page_node) override;
  void OnIsCapturingAudioChanged(const PageNode* page_node) override;
  void OnIsBeingMirroredChanged(const PageNode* page_node) override;
  void OnIsCapturingWindowChanged(const PageNode* page_node) override;
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override;
  void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) override;
  void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) override;
  // Pin and dev tools feature is not supported on Android. But
  // ProcessRankPolicyAndroid supports OnIsPinnedTabChanged()
  // OnIsDevToolsOpenChanged() to prevent this feature from being broken by
  // future adoption because DiscardEligibilityPolicy::CanDiscard() takes those
  // into account. Overriding the method does not have overhead since the
  // callbacks are never triggered.
  void OnIsPinnedTabChanged(const PageNode* page_node) override;
  void OnIsDevToolsOpenChanged(const PageNode* page_node) override;
  void OnUpdatedTitleOrFaviconInBackgroundChanged(
      const PageNode* page_node) override;

 private:
  const bool is_perceptible_importance_supported_;
  void UpdateProcessRank(const PageNode* page_node);
  content::ChildProcessImportance CalculateRank(const PageNode* page_node);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROCESS_RANK_POLICY_ANDROID_H_
