// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"
#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_rate_throttling.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#endif

using performance_manager::user_tuning::prefs::HighEfficiencyModeState;
using performance_manager::user_tuning::prefs::kHighEfficiencyModeState;

namespace performance_manager::user_tuning {
namespace {

UserPerformanceTuningManager* g_user_performance_tuning_manager = nullptr;

class HighEfficiencyModeDelegateImpl
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          HighEfficiencyModeDelegate {
 public:
  void ToggleHighEfficiencyMode(HighEfficiencyModeState state) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](HighEfficiencyModeState state) {
              auto* heuristic_memory_saver_policy =
                  policies::HeuristicMemorySaverPolicy::GetInstance();
              CHECK(heuristic_memory_saver_policy);
              auto* high_efficiency_mode_policy =
                  policies::HighEfficiencyModePolicy::GetInstance();
              CHECK(high_efficiency_mode_policy);
              switch (state) {
                case HighEfficiencyModeState::kDisabled:
                  heuristic_memory_saver_policy->SetActive(false);
                  high_efficiency_mode_policy->OnHighEfficiencyModeChanged(
                      false);
                  return;
                case HighEfficiencyModeState::kEnabled:
                  heuristic_memory_saver_policy->SetActive(true);
                  high_efficiency_mode_policy->OnHighEfficiencyModeChanged(
                      false);
                  return;
                case HighEfficiencyModeState::kEnabledOnTimer:
                  heuristic_memory_saver_policy->SetActive(false);
                  high_efficiency_mode_policy->OnHighEfficiencyModeChanged(
                      true);
                  return;
              }
              NOTREACHED_NORETURN();
            },
            state));
  }

  void SetTimeBeforeDiscard(base::TimeDelta time_before_discard) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce(
                       [](base::TimeDelta time_before_discard) {
                         auto* policy =
                             policies::HighEfficiencyModePolicy::GetInstance();
                         CHECK(policy);
                         policy->SetTimeBeforeDiscard(time_before_discard);
                       },
                       time_before_discard));
  }

  ~HighEfficiencyModeDelegateImpl() override = default;
};

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    UserPerformanceTuningManager::ResourceUsageTabHelper);

UserPerformanceTuningManager::ResourceUsageTabHelper::
    ~ResourceUsageTabHelper() = default;

void UserPerformanceTuningManager::ResourceUsageTabHelper::PrimaryPageChanged(
    content::Page&) {
  // Reset memory usage count when we navigate to another site since the
  // memory usage reported will be outdated.
  resource_usage_->set_memory_usage_in_bytes(0);
}

UserPerformanceTuningManager::ResourceUsageTabHelper::ResourceUsageTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<ResourceUsageTabHelper>(*contents),
      resource_usage_(base::MakeRefCounted<TabResourceUsage>()) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    UserPerformanceTuningManager::PreDiscardResourceUsage);

UserPerformanceTuningManager::PreDiscardResourceUsage::PreDiscardResourceUsage(
    content::WebContents* contents,
    uint64_t memory_footprint_estimate,
    ::mojom::LifecycleUnitDiscardReason discard_reason)
    : content::WebContentsUserData<PreDiscardResourceUsage>(*contents),
      memory_footprint_estimate_(memory_footprint_estimate),
      discard_reason_(discard_reason),
      discard_liveticks_(base::LiveTicks::Now()) {}

UserPerformanceTuningManager::PreDiscardResourceUsage::
    ~PreDiscardResourceUsage() = default;

// static
bool UserPerformanceTuningManager::HasInstance() {
  return g_user_performance_tuning_manager;
}

// static
UserPerformanceTuningManager* UserPerformanceTuningManager::GetInstance() {
  DCHECK(g_user_performance_tuning_manager);
  return g_user_performance_tuning_manager;
}

UserPerformanceTuningManager::~UserPerformanceTuningManager() {
  DCHECK_EQ(this, g_user_performance_tuning_manager);
  g_user_performance_tuning_manager = nullptr;
}

void UserPerformanceTuningManager::AddObserver(Observer* o) {
  observers_.AddObserver(o);
}

void UserPerformanceTuningManager::RemoveObserver(Observer* o) {
  observers_.RemoveObserver(o);
}

bool UserPerformanceTuningManager::IsHighEfficiencyModeActive() {
  HighEfficiencyModeState state = performance_manager::user_tuning::prefs::
      GetCurrentHighEfficiencyModeState(pref_change_registrar_.prefs());
  return state != HighEfficiencyModeState::kDisabled;
}

bool UserPerformanceTuningManager::IsHighEfficiencyModeManaged() const {
  auto* pref =
      pref_change_registrar_.prefs()->FindPreference(kHighEfficiencyModeState);
  return pref->IsManaged();
}

bool UserPerformanceTuningManager::IsHighEfficiencyModeDefault() const {
  auto* pref =
      pref_change_registrar_.prefs()->FindPreference(kHighEfficiencyModeState);
  return pref->IsDefaultValue();
}

void UserPerformanceTuningManager::SetHighEfficiencyModeEnabled(bool enabled) {
  HighEfficiencyModeState state = enabled
                                      ? HighEfficiencyModeState::kEnabledOnTimer
                                      : HighEfficiencyModeState::kDisabled;
  pref_change_registrar_.prefs()->SetInteger(kHighEfficiencyModeState,
                                             static_cast<int>(state));
}

UserPerformanceTuningManager::UserPerformanceTuningReceiverImpl::
    ~UserPerformanceTuningReceiverImpl() = default;

void UserPerformanceTuningManager::UserPerformanceTuningReceiverImpl::
    NotifyTabCountThresholdReached() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        // Hitting this CHECK would mean this task is running after
        // PostMainMessageLoopRun, which shouldn't happen.
        CHECK(g_user_performance_tuning_manager);
        GetInstance()->NotifyTabCountThresholdReached();
      }));
}

void UserPerformanceTuningManager::UserPerformanceTuningReceiverImpl::
    NotifyMemoryThresholdReached() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        // Hitting this CHECK would mean this task is running after
        // PostMainMessageLoopRun, which shouldn't happen.
        CHECK(g_user_performance_tuning_manager);
        GetInstance()->NotifyMemoryThresholdReached();
      }));
}

void UserPerformanceTuningManager::UserPerformanceTuningReceiverImpl::
    NotifyMemoryMetricsRefreshed(ProxyAndPmfKbVector proxies_and_pmf) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ProxyAndPmfKbVector web_contents_memory_usage) {
            if (base::FeatureList::IsEnabled(
                    performance_manager::features::kMemoryUsageInHovercards)) {
              for (const auto& [contents_proxy, pmf] :
                   web_contents_memory_usage) {
                content::WebContents* web_contents = contents_proxy.Get();
                if (web_contents) {
                  ResourceUsageTabHelper* helper =
                      ResourceUsageTabHelper::FromWebContents(web_contents);
                  if (helper) {
                    helper->SetMemoryUsageInBytes(pmf * 1024);
                  }
                }
              }
            }
            // Hitting this CHECK would mean this task is running after
            // PostMainMessageLoopRun, which shouldn't happen.
            CHECK(g_user_performance_tuning_manager);
            GetInstance()->NotifyMemoryMetricsRefreshed();
          },
          std::move(proxies_and_pmf)));
}

UserPerformanceTuningManager::UserPerformanceTuningManager(
    PrefService* local_state,
    std::unique_ptr<UserPerformanceTuningNotifier> notifier,
    std::unique_ptr<HighEfficiencyModeDelegate> high_efficiency_mode_delegate)
    : high_efficiency_mode_delegate_(
          high_efficiency_mode_delegate
              ? std::move(high_efficiency_mode_delegate)
              : std::make_unique<HighEfficiencyModeDelegateImpl>()) {
  DCHECK(!g_user_performance_tuning_manager);
  g_user_performance_tuning_manager = this;

  if (notifier) {
    performance_manager::PerformanceManager::PassToGraph(FROM_HERE,
                                                         std::move(notifier));
  }

  performance_manager::user_tuning::prefs::MigrateHighEfficiencyModePref(
      local_state);

  pref_change_registrar_.Init(local_state);
}

void UserPerformanceTuningManager::Start() {
  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::
          kHighEfficiencyModeTimeBeforeDiscardInMinutes,
      base::BindRepeating(&UserPerformanceTuningManager::
                              OnHighEfficiencyModeTimeBeforeDiscardChanged,
                          base::Unretained(this)));
  // Make sure the initial state of the discard timer pref is passed on to the
  // policy before it can be enabled, because the policy initially has a dummy
  // value for time_before_discard_. This prevents tabs' discard timers from
  // starting with a value different from the pref.
  OnHighEfficiencyModeTimeBeforeDiscardChanged();

  pref_change_registrar_.Add(
      kHighEfficiencyModeState,
      base::BindRepeating(
          &UserPerformanceTuningManager::OnHighEfficiencyModePrefChanged,
          base::Unretained(this)));
  // Make sure the initial state of the pref is passed on to the policy.
  OnHighEfficiencyModePrefChanged();
}

void UserPerformanceTuningManager::OnHighEfficiencyModePrefChanged() {
  HighEfficiencyModeState state =
      prefs::GetCurrentHighEfficiencyModeState(pref_change_registrar_.prefs());
  if (!base::FeatureList::IsEnabled(features::kHighEfficiencyMultistateMode)) {
    if (!IsHighEfficiencyModeManaged() &&
        base::FeatureList::IsEnabled(features::kForceHeuristicMemorySaver)) {
      // Set the heuristic policy for experimentation regardless of the pref.
      state = base::FeatureList::IsEnabled(features::kHeuristicMemorySaver)
                  ? HighEfficiencyModeState::kEnabled
                  : HighEfficiencyModeState::kDisabled;
    } else if (state != HighEfficiencyModeState::kDisabled) {
      // The user has enabled high efficiency mode, but without the multistate
      // UI they didn't choose a policy. The feature controls which policy to
      // use.
      state = base::FeatureList::IsEnabled(features::kHeuristicMemorySaver)
                  ? HighEfficiencyModeState::kEnabled
                  : HighEfficiencyModeState::kEnabledOnTimer;
    }
  }
  high_efficiency_mode_delegate_->ToggleHighEfficiencyMode(state);
  for (auto& obs : observers_) {
    obs.OnHighEfficiencyModeChanged();
  }
}

void UserPerformanceTuningManager::
    OnHighEfficiencyModeTimeBeforeDiscardChanged() {
  base::TimeDelta time_before_discard = performance_manager::user_tuning::
      prefs::GetCurrentHighEfficiencyModeTimeBeforeDiscard(
          pref_change_registrar_.prefs());
  high_efficiency_mode_delegate_->SetTimeBeforeDiscard(time_before_discard);
}

void UserPerformanceTuningManager::NotifyTabCountThresholdReached() {
  for (auto& obs : observers_) {
    obs.OnTabCountThresholdReached();
  }
}

void UserPerformanceTuningManager::NotifyMemoryThresholdReached() {
  for (auto& obs : observers_) {
    obs.OnMemoryThresholdReached();
  }
}

void UserPerformanceTuningManager::NotifyMemoryMetricsRefreshed() {
  for (auto& obs : observers_) {
    obs.OnMemoryMetricsRefreshed();
  }
}

void UserPerformanceTuningManager::DiscardPageForTesting(
    content::WebContents* web_contents) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             base::WeakPtr<performance_manager::PageNode> page_node,
             performance_manager::Graph* graph) {
            if (page_node) {
              performance_manager::policies::PageDiscardingHelper::GetFromGraph(
                  graph)
                  ->ImmediatelyDiscardSpecificPage(
                      page_node.get(),
                      ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
              quit_closure.Run();
            }
          },
          run_loop.QuitClosure(),
          performance_manager::PerformanceManager::
              GetPrimaryPageNodeForWebContents(web_contents)));
  run_loop.Run();
}

}  // namespace performance_manager::user_tuning
