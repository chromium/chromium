// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/swap_pressure_discarding_policy_android.h"

#include <sys/system_properties.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

namespace performance_manager::policies {

namespace {

constexpr base::TimeDelta kMinCheckInterval = base::Milliseconds(2000);
constexpr base::TimeDelta kFollowUpRecheckDelay = base::Seconds(3);
constexpr base::TimeDelta kMinTimeInBackground = base::Seconds(10);
constexpr int kMaxDiscardsPerCheck = 2;

constexpr int kDefaultSwapFreeThresholdPct = 15;
constexpr int kSwapFreeHeadroomOverLmkdPct = 5;
constexpr int kMinDerivedSwapFreeThresholdPct = 10;
constexpr int kMaxDerivedSwapFreeThresholdPct = 30;

constexpr int kDefaultFileCacheThresholdRamPct = 7;
constexpr base::ByteSize kMinDefaultFileCacheThreshold = base::MiB(300);
constexpr base::ByteSize kMaxDefaultFileCacheThreshold = base::MiB(600);
constexpr int kMinDefaultTabsToKeep = 2;
constexpr int kMaxDefaultTabsToKeep = 8;

std::optional<unsigned int> ReadSystemPropertyUint(const char* name) {
  char value[PROP_VALUE_MAX] = {};
  if (__system_property_get(name, value) <= 0) {
    return std::nullopt;
  }
  unsigned int parsed = 0;
  if (!base::StringToUint(value, &parsed)) {
    return std::nullopt;
  }
  return parsed;
}

std::optional<unsigned int> GetLmkdSwapFreeLowPercentage() {
  static const std::optional<unsigned int> kVal =
      ReadSystemPropertyUint("ro.lmk.swap_free_low_percentage");
  return kVal;
}

std::optional<base::ByteSize> GetLmkdFilecacheMin() {
  static const std::optional<base::ByteSize> kVal =
      []() -> std::optional<base::ByteSize> {
    if (auto val = ReadSystemPropertyUint("ro.lmk.filecache_min_kb")) {
      return base::KiB(*val);
    }
    return std::nullopt;
  }();
  return kVal;
}

bool IsLiveTabNode(const PageNode* page_node) {
  if (page_node->GetType() != PageType::kTab ||
      !page_node->GetPrimaryMainFrameNode()) {
    return false;
  }
  const auto* live_state_data =
      PageLiveStateDecorator::Data::FromPageNode(page_node);
  if (live_state_data && live_state_data->IsDiscarded()) {
    return false;
  }
  return true;
}

}  // namespace

SwapPressureDiscardingPolicyAndroid::SwapPressureDiscardingPolicyAndroid()
    : SwapPressureDiscardingPolicyAndroid(
          base::BindRepeating(&base::GetSystemMemoryInfo)) {}

SwapPressureDiscardingPolicyAndroid::SwapPressureDiscardingPolicyAndroid(
    MemoryInfoProvider memory_info_provider)
    : memory_info_provider_(std::move(memory_info_provider)) {}

SwapPressureDiscardingPolicyAndroid::~SwapPressureDiscardingPolicyAndroid() =
    default;

// static
SwapPressureDiscardingPolicyAndroid::EffectiveThresholds
SwapPressureDiscardingPolicyAndroid::ComputeEffectiveThresholds(
    const base::SystemMemoryInfo& meminfo) {
  CHECK(meminfo.total.is_positive());
  CHECK(meminfo.swap_total.is_positive());

  EffectiveThresholds result;
  result.swap_free_pct = base::checked_cast<int>(
      (meminfo.swap_free.InBytesF() / meminfo.swap_total.InBytesF()) * 100);
  result.file_cache = meminfo.active_file + meminfo.inactive_file;

  // 1. Swap pressure gate:
  //    If `swap_free_threshold_pct` > 0, use that explicit Finch override;
  //    otherwise derive from lmkd's `ro.lmk.swap_free_low_percentage` system
  //    property (adding 5 percentage points of headroom, clamped to [10%, 30%],
  //    defaulting to 15% when the property is unset).
  const int explicit_swap_free_pct =
      chrome::android::kAndroidSwapPressureSwapFreeThresholdPct.Get();
  int effective_swap_free_threshold_pct = explicit_swap_free_pct;
  if (explicit_swap_free_pct <= 0) {
    const std::optional<unsigned int> lmkd_swap_low_pct =
        GetLmkdSwapFreeLowPercentage();
    effective_swap_free_threshold_pct =
        lmkd_swap_low_pct.has_value()
            ? std::clamp<int>(base::ClampAdd(*lmkd_swap_low_pct,
                                             kSwapFreeHeadroomOverLmkdPct),
                              kMinDerivedSwapFreeThresholdPct,
                              kMaxDerivedSwapFreeThresholdPct)
            : kDefaultSwapFreeThresholdPct;
  }
  result.swap_under_pressure =
      result.swap_free_pct <= effective_swap_free_threshold_pct;

  // 2. File cache gate:
  //    If `file_cache_threshold_mb` > 0, use that explicit override; otherwise
  //    scale with MemTotal (7% of MemTotal, or 2x `ro.lmk.filecache_min_kb` if
  //    configured higher by the OEM, clamped to [300 MiB, 600 MiB]).
  const int explicit_file_cache_mb =
      chrome::android::kAndroidSwapPressureFileCacheThresholdMb.Get();
  if (explicit_file_cache_mb > 0) {
    result.effective_file_cache_threshold =
        base::MiB(base::checked_cast<uint64_t>(explicit_file_cache_mb));
  } else {
    const base::ByteSize scaled =
        meminfo.total * (kDefaultFileCacheThresholdRamPct / 100.0);
    const base::ByteSize lmkd_floor =
        GetLmkdFilecacheMin().value_or(base::ByteSize()) * 2;
    result.effective_file_cache_threshold =
        std::clamp(std::max(scaled, lmkd_floor), kMinDefaultFileCacheThreshold,
                   kMaxDefaultFileCacheThreshold);
  }
  CHECK(result.effective_file_cache_threshold.is_positive());

  // 3. Minimum live tabs floor:
  //    If `min_tabs_to_keep` > 0, use that explicit override; otherwise scale
  //    with physical RAM tier (ceil(MemTotal in GiB) / 2, clamped to [2, 8],
  //    since /proc/meminfo MemTotal excludes kernel/modem carveouts).
  const int explicit_min_tabs =
      chrome::android::kAndroidSwapPressureMinTabsToKeep.Get();
  if (explicit_min_tabs > 0) {
    result.effective_min_tabs_to_keep = explicit_min_tabs;
  } else {
    const int approx_ram_gib =
        base::saturated_cast<int>(std::ceil(meminfo.total.InGiBF()));
    result.effective_min_tabs_to_keep = std::clamp(
        approx_ram_gib / 2, kMinDefaultTabsToKeep, kMaxDefaultTabsToKeep);
  }
  CHECK_GE(result.effective_min_tabs_to_keep, 1);

  return result;
}

void SwapPressureDiscardingPolicyAndroid::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
  app_status_listener_ =
      base::android::ApplicationStatusListener::New(base::BindRepeating(
          &SwapPressureDiscardingPolicyAndroid::OnApplicationStateChange,
          base::Unretained(this)));
}

void SwapPressureDiscardingPolicyAndroid::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  deferred_check_timer_.Stop();
  app_status_listener_.reset();
  FlushSessionEndedOutcomes();
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
}

void SwapPressureDiscardingPolicyAndroid::OnFrameNodeAdded(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeScheduleCheck();
}

void SwapPressureDiscardingPolicyAndroid::OnPageNodeAdded(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeScheduleCheck();
}

void SwapPressureDiscardingPolicyAndroid::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = discarded_pages_.find(page_node);
  if (it != discarded_pages_.end()) {
    base::UmaHistogramEnumeration(
        "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome",
        DiscardedTabSessionOutcome::kClosedWithoutRevisit);
    discarded_pages_.erase(it);
  }
}

void SwapPressureDiscardingPolicyAndroid::OnMainFrameDocumentChanged(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeScheduleCheck();
}

void SwapPressureDiscardingPolicyAndroid::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeScheduleCheck();
}

void SwapPressureDiscardingPolicyAndroid::OnIsVisibleChanged(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (page_node->IsVisible()) {
    auto it = discarded_pages_.find(page_node);
    if (it != discarded_pages_.end()) {
      base::UmaHistogramCustomTimes(
          "PerformanceManager.AndroidSwapPressure.DiscardedTabTimeToRevisit",
          base::TimeTicks::Now() - it->second, base::Seconds(1), base::Days(1),
          50);
      base::UmaHistogramEnumeration(
          "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome",
          DiscardedTabSessionOutcome::kRevisitedInForegroundSession);
      discarded_pages_.erase(it);
    }
  }
  MaybeScheduleCheck();
}

void SwapPressureDiscardingPolicyAndroid::OnApplicationStateChange(
    base::android::ApplicationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/574606241): Consider centralizing foreground-session-end
  // notifications across PerformanceManager policies.
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    MaybeScheduleCheck();
  } else if (state == base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES ||
             state ==
                 base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES) {
    FlushSessionEndedOutcomes();
  }
}

void SwapPressureDiscardingPolicyAndroid::FlushSessionEndedOutcomes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < discarded_pages_.size(); ++i) {
    base::UmaHistogramEnumeration(
        "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome",
        DiscardedTabSessionOutcome::kSessionEndedWithoutRevisit);
  }
  discarded_pages_.clear();
}

void SwapPressureDiscardingPolicyAndroid::MaybeScheduleCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta elapsed = last_check_ticks_.is_null()
                                      ? kMinCheckInterval
                                      : (now - last_check_ticks_);
  const base::TimeDelta delay = elapsed >= kMinCheckInterval
                                    ? base::TimeDelta()
                                    : (kMinCheckInterval - elapsed);

  if (!deferred_check_timer_.IsRunning() ||
      deferred_check_timer_.desired_run_time() > now + delay) {
    deferred_check_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&SwapPressureDiscardingPolicyAndroid::CheckSwapPressure,
                       base::Unretained(this), /*schedule_followup=*/true));
  }
}

bool SwapPressureDiscardingPolicyAndroid::HasLoadingTabAboveKeepFloor(
    int min_tabs_to_keep) const {
  int live_tab_count = 0;
  bool has_loading_tab = false;
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    if (!IsLiveTabNode(page_node)) {
      continue;
    }
    ++live_tab_count;
    const PageNode::LoadingState state = page_node->GetLoadingState();
    if (state == PageNode::LoadingState::kLoading ||
        state == PageNode::LoadingState::kLoadedBusy) {
      has_loading_tab = true;
    }
  }
  return has_loading_tab && live_tab_count > min_tabs_to_keep;
}

bool SwapPressureDiscardingPolicyAndroid::DiscardUnderSwapPressure(
    PageDiscardingHelper* discarding_helper,
    DiscardEligibilityPolicy* eligibility_policy,
    const base::SystemMemoryInfo& meminfo,
    const EffectiveThresholds& thresholds) {
  const int min_tabs_to_keep = thresholds.effective_min_tabs_to_keep;
  CHECK_GE(min_tabs_to_keep, 1);

  auto all_page_nodes = GetOwningGraph()->GetAllPageNodes();
  std::vector<const PageNode*> live_tabs;
  live_tabs.reserve(all_page_nodes.size());
  for (const PageNode* page_node : all_page_nodes) {
    if (IsLiveTabNode(page_node)) {
      live_tabs.push_back(page_node);
    }
  }

  const int live_tab_count = static_cast<int>(live_tabs.size());
  base::UmaHistogramCounts100(
      "PerformanceManager.AndroidSwapPressure.LiveTabCountAtPressure",
      live_tab_count);
  if (live_tab_count <= min_tabs_to_keep) {
    base::UmaHistogramEnumeration(
        "PerformanceManager.AndroidSwapPressure.CheckOutcome",
        CheckOutcome::kBlockedByMinTabsFloor);
    return false;
  }

  const base::TimeTicks now = base::TimeTicks::Now();

  bool has_aging_background_tab = false;
  std::vector<PageNodeSortProxy> candidates;
  candidates.reserve(live_tabs.size());
  for (const PageNode* page_node : live_tabs) {
    if (!page_node->IsVisible() &&
        (now - page_node->GetLastVisibilityChangeTime()) <
            kMinTimeInBackground) {
      has_aging_background_tab = true;
    }

    CanDiscardResult can_discard =
        eligibility_policy->CanDiscardWithCustomRecentVisibilityWindow(
            page_node, DiscardEligibilityPolicy::DiscardReason::URGENT,
            kMinTimeInBackground);
    if (can_discard != CanDiscardResult::kEligible) {
      continue;
    }
    candidates.emplace_back(page_node->GetWeakPtr(), can_discard,
                            page_node->IsVisible(), page_node->IsFocused(),
                            page_node->GetLastVisibilityChangeTime());
  }

  if (candidates.empty()) {
    base::UmaHistogramEnumeration(
        "PerformanceManager.AndroidSwapPressure.CheckOutcome",
        has_aging_background_tab
            ? CheckOutcome::kNoEligibleCandidatesAging
            : CheckOutcome::kNoEligibleCandidatesProtected);
    return has_aging_background_tab;
  }

  // Sort descending so least important (oldest backgrounded) is at the end.
  std::sort(candidates.rbegin(), candidates.rend());

  const int max_allowed_by_keep_floor = live_tab_count - min_tabs_to_keep;
  const int target_discards =
      std::min({static_cast<int>(candidates.size()), kMaxDiscardsPerCheck,
                max_allowed_by_keep_floor});

  struct DiscardCandidateInfo {
    base::WeakPtr<const PageNode> page_node;
    base::TimeDelta time_in_bg;
    base::ByteSize estimated_footprint;
  };
  std::vector<const PageNode*> to_discard;
  std::vector<DiscardCandidateInfo> candidate_infos;
  base::ByteSize target_estimated_reclaimed;
  to_discard.reserve(target_discards);
  candidate_infos.reserve(target_discards);
  for (int i = 0; i < target_discards; ++i) {
    base::WeakPtr<const PageNode> weak_node = candidates.back().page_node();
    candidates.pop_back();
    CHECK(weak_node);
    const PageNode* node = weak_node.get();
    const base::ByteSize footprint =
        user_tuning::GetDiscardedMemoryEstimateForPage(node);
    to_discard.push_back(node);
    target_estimated_reclaimed += footprint;
    candidate_infos.push_back(DiscardCandidateInfo{
        .page_node = std::move(weak_node),
        .time_in_bg = now - node->GetLastVisibilityChangeTime(),
        .estimated_footprint = footprint,
    });
  }

  TRACE_EVENT(
      "browser", "SwapPressureDiscardingPolicyAndroid::DiscardTabs",
      "live_tab_count", live_tab_count, "min_tabs_to_keep", min_tabs_to_keep,
      "target_discards", static_cast<int>(to_discard.size()), "swap_free_pct",
      thresholds.swap_free_pct, "swap_free_mb", meminfo.swap_free.InMiB(),
      "file_cache_mb", thresholds.file_cache.InMiB(), "estimated_reclaimed_mb",
      target_estimated_reclaimed.InMiB());

  LOG(WARNING) << "AndroidSwapPressure: SwapFree=" << thresholds.swap_free_pct
               << "% (" << meminfo.swap_free.InMiB() << " MiB), "
               << "FileCache=" << thresholds.file_cache.InMiB() << " MiB, "
               << "live_tabs=" << live_tab_count << "; discarding "
               << to_discard.size() << " oldest background tab(s).";

  const bool discarded = discarding_helper->ImmediatelyDiscardMultiplePages(
      to_discard, DiscardEligibilityPolicy::DiscardReason::URGENT,
      /*ignore_recent_visibility=*/true, /*eligibility_policy=*/nullptr);

  int discarded_count = 0;
  base::ByteSize estimated_reclaimed;
  if (discarded) {
    for (const DiscardCandidateInfo& info : candidate_infos) {
      if (!info.page_node || !IsLiveTabNode(info.page_node.get())) {
        ++discarded_count;
        estimated_reclaimed += info.estimated_footprint;
        base::UmaHistogramCustomTimes(
            "PerformanceManager.AndroidSwapPressure."
            "DiscardedTabTimeInBackground",
            info.time_in_bg, base::Seconds(1), base::Days(1), 50);
        if (info.page_node) {
          discarded_pages_[info.page_node.get()] = now;
        }
      }
    }
  }
  const bool any_discarded = discarded_count > 0;
  base::UmaHistogramCounts100(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabCount",
      discarded_count);
  base::UmaHistogramEnumeration(
      "PerformanceManager.AndroidSwapPressure.CheckOutcome",
      any_discarded ? CheckOutcome::kDiscardedTabs
                    : CheckOutcome::kDiscardFailed);

  if (any_discarded) {
    base::UmaHistogramMemoryLargeMB(
        "PerformanceManager.AndroidSwapPressure.EstimatedReclaimedMb",
        base::saturated_cast<int>(estimated_reclaimed.InMiB()));
  }

  const int remaining_live_tabs = live_tab_count - discarded_count;
  if (remaining_live_tabs <= min_tabs_to_keep) {
    return false;
  }
  return !candidates.empty() || has_aging_background_tab;
}

void SwapPressureDiscardingPolicyAndroid::CheckSwapPressure(
    bool schedule_followup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  deferred_check_timer_.Stop();
  last_check_ticks_ = base::TimeTicks::Now();

  PageDiscardingHelper* discarding_helper =
      PageDiscardingHelper::GetFromGraph(GetOwningGraph());
  DiscardEligibilityPolicy* eligibility_policy =
      DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());
  if (!discarding_helper || !eligibility_policy) {
    return;
  }

  base::SystemMemoryInfo meminfo;
  if (!memory_info_provider_.Run(&meminfo) || meminfo.total.is_zero() ||
      meminfo.swap_total.is_zero()) {
    return;
  }

  const EffectiveThresholds thresholds = ComputeEffectiveThresholds(meminfo);
  const bool swap_under_pressure = thresholds.swap_under_pressure;
  const bool file_cache_under_pressure = thresholds.file_cache_under_pressure();
  if (swap_under_pressure || file_cache_under_pressure) {
    base::UmaHistogramPercentage(
        "PerformanceManager.AndroidSwapPressure.SwapFreePercent",
        thresholds.swap_free_pct);
    base::UmaHistogramMemoryLargeMB(
        "PerformanceManager.AndroidSwapPressure.FileCacheMb",
        base::saturated_cast<int>(thresholds.file_cache.InMiB()));
  }

  const bool under_pressure = swap_under_pressure && file_cache_under_pressure;
  TRACE_EVENT("browser",
              "SwapPressureDiscardingPolicyAndroid::CheckSwapPressure",
              "swap_free_pct", thresholds.swap_free_pct, "file_cache_mb",
              thresholds.file_cache.InMiB(), "under_pressure", under_pressure);
  TRACE_COUNTER("browser", "AndroidSwapPressure.SwapFreeMb",
                meminfo.swap_free.InMiB());
  TRACE_COUNTER("browser", "AndroidSwapPressure.FileCacheMb",
                thresholds.file_cache.InMiB());

  bool can_discard_more = false;
  if (under_pressure) {
    can_discard_more = DiscardUnderSwapPressure(
        discarding_helper, eligibility_policy, meminfo, thresholds);
  } else {
    CheckOutcome outcome = CheckOutcome::kHealthy;
    if (swap_under_pressure) {
      outcome = CheckOutcome::kSwapPressureOnly;
    } else if (file_cache_under_pressure) {
      outcome = CheckOutcome::kFileCachePressureOnly;
    }
    base::UmaHistogramEnumeration(
        "PerformanceManager.AndroidSwapPressure.CheckOutcome", outcome);
  }

  // Discarding a page can synchronously reset in-progress navigations and fire
  // OnLoadingStateChanged() -> MaybeScheduleCheck(). Stop any timer re-armed
  // during DiscardUnderSwapPressure() before deciding whether a follow-up check
  // is needed.
  deferred_check_timer_.Stop();

  // Schedule a coalesced follow-up check only while tabs are actively loading
  // (with live tabs above the keep floor) or while swap pressure persists and
  // further tabs can still be discarded. Zero background timers when idle.
  if (schedule_followup &&
      ((under_pressure && can_discard_more) ||
       HasLoadingTabAboveKeepFloor(thresholds.effective_min_tabs_to_keep))) {
    deferred_check_timer_.Start(
        FROM_HERE, kFollowUpRecheckDelay,
        base::BindOnce(&SwapPressureDiscardingPolicyAndroid::CheckSwapPressure,
                       base::Unretained(this), /*schedule_followup=*/true));
  }
}

}  // namespace performance_manager::policies
