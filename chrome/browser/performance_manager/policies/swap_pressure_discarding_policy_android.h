// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_SWAP_PRESSURE_DISCARDING_POLICY_ANDROID_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_SWAP_PRESSURE_DISCARDING_POLICY_ANDROID_H_

#include <memory>

#include "base/android/application_status_listener.h"
#include "base/byte_size.h"
#include "base/functional/callback.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager::policies {

class DiscardEligibilityPolicy;
class PageDiscardingHelper;

// Proactively discards the oldest background tabs on Android when system swap
// (ZRAM) approaches exhaustion and file page cache is low, before lmkd's
// swap_free_low_percentage gate triggers a page-cache collapse and thrashing.
//
// Reads /proc/meminfo on page, frame, and app lifecycle events (tab added,
// frame/subframe added, main frame navigation committed, loading state changed,
// visibility changed, app foregrounded), rate-limited to at most once every 2
// seconds. While any tab is actively loading or further background tabs remain
// discardable under swap pressure, a one-shot follow-up check is scheduled;
// there is zero background polling when Chrome is idle.
class SwapPressureDiscardingPolicyAndroid : public GraphOwned,
                                            public PageNodeObserver,
                                            public FrameNodeObserver {
 public:
  using MemoryInfoProvider =
      base::RepeatingCallback<bool(base::SystemMemoryInfo*)>;

  struct EffectiveThresholds {
    bool file_cache_under_pressure() const {
      return file_cache <= effective_file_cache_threshold;
    }

    bool swap_under_pressure = false;
    int swap_free_pct = 0;
    base::ByteSize file_cache;
    base::ByteSize effective_file_cache_threshold;
    int effective_min_tabs_to_keep = 0;
  };

  // Outcome of each SwapPressureDiscardingPolicyAndroid evaluation.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(AndroidSwapPressureCheckOutcome)
  enum class CheckOutcome {
    kHealthy = 0,
    kSwapPressureOnly = 1,
    kFileCachePressureOnly = 2,
    kBlockedByMinTabsFloor = 3,
    kNoEligibleCandidatesAging = 4,
    kNoEligibleCandidatesProtected = 5,
    kDiscardedTabs = 6,
    kDiscardFailed = 7,
    kMaxValue = kDiscardFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/performance_manager/enums.xml:AndroidSwapPressureCheckOutcome)

  // Session-bounded fate of each background tab proactively discarded by
  // SwapPressureDiscardingPolicyAndroid. Recorded once per discarded tab when
  // the user switches back to it, closes it, or backgrounds Chrome (ending the
  // visible session) without revisiting it.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(AndroidSwapPressureDiscardedTabSessionOutcome)
  enum class DiscardedTabSessionOutcome {
    kRevisitedInForegroundSession = 0,
    kClosedWithoutRevisit = 1,
    kSessionEndedWithoutRevisit = 2,
    kMaxValue = kSessionEndedWithoutRevisit,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/performance_manager/enums.xml:AndroidSwapPressureDiscardedTabSessionOutcome)

  static EffectiveThresholds ComputeEffectiveThresholds(
      const base::SystemMemoryInfo& meminfo);

  SwapPressureDiscardingPolicyAndroid();
  explicit SwapPressureDiscardingPolicyAndroid(
      MemoryInfoProvider memory_info_provider);
  ~SwapPressureDiscardingPolicyAndroid() override;

  SwapPressureDiscardingPolicyAndroid(
      const SwapPressureDiscardingPolicyAndroid&) = delete;
  SwapPressureDiscardingPolicyAndroid& operator=(
      const SwapPressureDiscardingPolicyAndroid&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnMainFrameDocumentChanged(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;

  void CheckSwapPressureForTesting(bool schedule_followup = false) {
    CheckSwapPressure(schedule_followup);
  }
  bool IsDeferredCheckTimerRunningForTesting() const {
    return deferred_check_timer_.IsRunning();
  }

 private:
  void MaybeScheduleCheck();
  void CheckSwapPressure(bool schedule_followup);
  // Returns true if another follow-up check under pressure can still discard
  // additional background tabs (either because more eligible candidates remain
  // above the keep floor, or because recently hidden background tabs are still
  // aging through `min_time_in_background_sec`).
  bool DiscardUnderSwapPressure(PageDiscardingHelper* discarding_helper,
                                DiscardEligibilityPolicy* eligibility_policy,
                                const base::SystemMemoryInfo& meminfo,
                                const EffectiveThresholds& thresholds);
  bool HasLoadingTabAboveKeepFloor(int min_tabs_to_keep) const;
  void OnApplicationStateChange(base::android::ApplicationState state);
  void FlushSessionEndedOutcomes();

  MemoryInfoProvider memory_info_provider_;
  base::TimeTicks last_check_ticks_;
  base::OneShotTimer deferred_check_timer_;
  absl::flat_hash_map<const PageNode*, base::TimeTicks> discarded_pages_;
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_SWAP_PRESSURE_DISCARDING_POLICY_ANDROID_H_
