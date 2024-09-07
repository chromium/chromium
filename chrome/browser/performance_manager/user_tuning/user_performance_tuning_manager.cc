// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/policies/memory_saver_mode_policy.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/metrics/page_resource_monitor.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_rate_throttling.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#endif

using performance_manager::user_tuning::prefs::kMemorySaverModeState;
using performance_manager::user_tuning::prefs::MemorySaverModeState;

namespace performance_manager::user_tuning {
namespace {

UserPerformanceTuningManager* g_user_performance_tuning_manager = nullptr;

class MemorySaverModeDelegateImpl
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          MemorySaverModeDelegate {
 public:
  void ToggleMemorySaverMode(MemorySaverModeState state) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](MemorySaverModeState state) {
              auto* memory_saver_mode_policy =
                  policies::MemorySaverModePolicy::GetInstance();
              CHECK(memory_saver_mode_policy);
              switch (state) {
                case MemorySaverModeState::kDisabled:
                  memory_saver_mode_policy->OnMemorySaverModeChanged(false);
                  return;
                case MemorySaverModeState::kEnabled:
                // The kDeprecated setting is being migrated to kEnabled so
                // treat them the same.
                case MemorySaverModeState::kDeprecated:
                  memory_saver_mode_policy->OnMemorySaverModeChanged(true);
                  return;
              }
              NOTREACHED();
            },
            state));
  }

  void SetMode(prefs::MemorySaverModeAggressiveness mode) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce(
                       [](prefs::MemorySaverModeAggressiveness mode) {
                         auto* policy =
                             policies::MemorySaverModePolicy::GetInstance();
                         CHECK(policy);
                         policy->SetMode(mode);
                       },
                       mode));
  }

  ~MemorySaverModeDelegateImpl() override = default;
};

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    UserPerformanceTuningManager::PreDiscardResourceUsage);

UserPerformanceTuningManager::PreDiscardResourceUsage::PreDiscardResourceUsage(
    content::WebContents* contents,
    uint64_t memory_footprint_estimate,
    ::mojom::LifecycleUnitDiscardReason discard_reason)
    : content::WebContentsUserData<PreDiscardResourceUsage>(*contents),
      memory_footprint_estimate_(memory_footprint_estimate),
      discard_reason_(discard_reason),
      discard_live_ticks_(base::LiveTicks::Now()) {}

UserPerformanceTuningManager::PreDiscardResourceUsage::
    ~PreDiscardResourceUsage() = default;

void UserPerformanceTuningManager::PreDiscardResourceUsage::UpdateDiscardInfo(
    uint64_t memory_footprint_estimate_kb,
    ::mojom::LifecycleUnitDiscardReason discard_reason,
    base::LiveTicks discard_live_ticks) {
  memory_footprint_estimate_ = memory_footprint_estimate_kb;
  discard_reason_ = discard_reason;
  discard_live_ticks_ = discard_live_ticks;
}

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

bool UserPerformanceTuningManager::IsMemorySaverModeActive() {
  MemorySaverModeState state =
      performance_manager::user_tuning::prefs::GetCurrentMemorySaverModeState(
          pref_change_registrar_.prefs());
  return state != MemorySaverModeState::kDisabled;
}

bool UserPerformanceTuningManager::IsMemorySaverModeManaged() const {
  auto* pref =
      pref_change_registrar_.prefs()->FindPreference(kMemorySaverModeState);
  return pref->IsManaged();
}

bool UserPerformanceTuningManager::IsMemorySaverModeDefault() const {
  auto* pref =
      pref_change_registrar_.prefs()->FindPreference(kMemorySaverModeState);
  return pref->IsDefaultValue();
}

void UserPerformanceTuningManager::SetMemorySaverModeEnabled(bool enabled) {
  MemorySaverModeState state = enabled ? MemorySaverModeState::kEnabled
                                       : MemorySaverModeState::kDisabled;
  pref_change_registrar_.prefs()->SetInteger(kMemorySaverModeState,
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

UserPerformanceTuningManager::UserPerformanceTuningManager(
    PrefService* local_state,
    std::unique_ptr<UserPerformanceTuningNotifier> notifier,
    std::unique_ptr<MemorySaverModeDelegate> memory_saver_mode_delegate)
    : memory_saver_mode_delegate_(
          memory_saver_mode_delegate
              ? std::move(memory_saver_mode_delegate)
              : std::make_unique<MemorySaverModeDelegateImpl>()) {
  DCHECK(!g_user_performance_tuning_manager);
  g_user_performance_tuning_manager = this;

  if (notifier) {
    performance_manager::PerformanceManager::PassToGraph(FROM_HERE,
                                                         std::move(notifier));
  }

  performance_manager::user_tuning::prefs::MigrateMemorySaverModePref(
      local_state);

  pref_change_registrar_.Init(local_state);
}

void UserPerformanceTuningManager::Start() {
  pref_change_registrar_.Add(
      kMemorySaverModeState,
      base::BindRepeating(
          &UserPerformanceTuningManager::OnMemorySaverModePrefChanged,
          base::Unretained(this)));
  // Make sure the initial state of the pref is passed on to the policy.
  UpdateMemorySaverModeState();

  pref_change_registrar_.Add(
      prefs::kMemorySaverModeAggressiveness,
      base::BindRepeating(
          &UserPerformanceTuningManager::OnMemorySaverAggressivenessPrefChanged,
          base::Unretained(this)));
  // Make sure the initial state of the pref is passed on to the policy.
  OnMemorySaverAggressivenessPrefChanged();
}

void UserPerformanceTuningManager::UpdateMemorySaverModeState() {
  MemorySaverModeState state =
      prefs::GetCurrentMemorySaverModeState(pref_change_registrar_.prefs());
  if (state != MemorySaverModeState::kDisabled) {
    // The user has enabled memory saver mode, but without the multistate
    // UI they didn't choose a policy. The feature controls which policy to
    // use.
    state = MemorySaverModeState::kEnabled;
  }
  memory_saver_mode_delegate_->ToggleMemorySaverMode(state);
}

void UserPerformanceTuningManager::OnMemorySaverModePrefChanged() {
  UpdateMemorySaverModeState();
  for (auto& obs : observers_) {
    obs.OnMemorySaverModeChanged();
  }
}

void UserPerformanceTuningManager::OnMemorySaverAggressivenessPrefChanged() {
  prefs::MemorySaverModeAggressiveness mode =
      prefs::GetCurrentMemorySaverMode(pref_change_registrar_.prefs());
  memory_saver_mode_delegate_->SetMode(mode);
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

void UserPerformanceTuningManager::DiscardPageForTesting(
    content::WebContents* web_contents) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  // The RunLoop is quit after discarding is executed on the main thread, so the
  // caller can check if discarding succeeded via WebContents::WasDiscarded().
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::ScopedClosureRunner quit_closure,
             base::WeakPtr<performance_manager::PageNode> page_node,
             performance_manager::Graph* graph) {
            if (page_node) {
              performance_manager::policies::PageDiscardingHelper::GetFromGraph(
                  graph)
                  ->ImmediatelyDiscardMultiplePages(
                      {page_node.get()},
                      ::mojom::LifecycleUnitDiscardReason::PROACTIVE,
                      base::DoNothingWithBoundArgs(std::move(quit_closure)));
            }
          },
          base::ScopedClosureRunner(run_loop.QuitClosure()),
          performance_manager::PerformanceManager::
              GetPrimaryPageNodeForWebContents(web_contents)));
  run_loop.Run();
}

}  // namespace performance_manager::user_tuning
