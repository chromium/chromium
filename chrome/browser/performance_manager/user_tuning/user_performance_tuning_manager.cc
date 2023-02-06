// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_rate_throttling.h"

namespace performance_manager::user_tuning {
namespace {

UserPerformanceTuningManager* g_user_performance_tuning_manager = nullptr;

constexpr base::TimeDelta kBatteryUsageWriteFrequency = base::Days(1);

class FrameThrottlingDelegateImpl
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          FrameThrottlingDelegate {
 public:
  void StartThrottlingAllFrameSinks() override {
    content::StartThrottlingAllFrameSinks(base::Hertz(30));
    NotifyPageTimelineMonitor(/*battery_saver_mode_enabled=*/true);
  }

  void StopThrottlingAllFrameSinks() override {
    content::StopThrottlingAllFrameSinks();
    NotifyPageTimelineMonitor(/*battery_saver_mode_enabled=*/false);
  }

  ~FrameThrottlingDelegateImpl() override = default;

 private:
  void NotifyPageTimelineMonitor(bool battery_saver_mode_enabled) {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](bool enabled, performance_manager::Graph* graph) {
              auto* monitor = graph->GetRegisteredObjectAs<
                  performance_manager::metrics::PageTimelineMonitor>();
              // It's possible for this to be null if the PageTimeline finch
              // feature is disabled.
              if (monitor) {
                monitor->SetBatterySaverEnabled(enabled);
              }
            },
            battery_saver_mode_enabled));
  }
};

class HighEfficiencyModeToggleDelegateImpl
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          HighEfficiencyModeToggleDelegate {
 public:
  void ToggleHighEfficiencyMode(bool enabled) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce(
                       [](bool enabled, performance_manager::Graph* graph) {
                         policies::HighEfficiencyModePolicy::GetInstance()
                             ->OnHighEfficiencyModeChanged(enabled);
                       },
                       enabled));
  }

  ~HighEfficiencyModeToggleDelegateImpl() override = default;
};

}  // namespace

const uint64_t UserPerformanceTuningManager::kLowBatteryThresholdPercent = 20;

const char UserPerformanceTuningManager::kForceDeviceHasBattery[] =
    "force-device-has-battery";

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    UserPerformanceTuningManager::PreDiscardResourceUsage);

UserPerformanceTuningManager::PreDiscardResourceUsage::PreDiscardResourceUsage(
    content::WebContents* contents,
    uint64_t memory_footprint_estimate)
    : content::WebContentsUserData<PreDiscardResourceUsage>(*contents),
      memory_footprint_estimate_(memory_footprint_estimate) {}

UserPerformanceTuningManager::PreDiscardResourceUsage::
    ~PreDiscardResourceUsage() = default;

// static
UserPerformanceTuningManager* UserPerformanceTuningManager::GetInstance() {
  DCHECK(g_user_performance_tuning_manager);
  return g_user_performance_tuning_manager;
}

UserPerformanceTuningManager::~UserPerformanceTuningManager() {
  DCHECK_EQ(this, g_user_performance_tuning_manager);
  g_user_performance_tuning_manager = nullptr;

  base::PowerMonitor::RemovePowerStateObserver(this);
}

void UserPerformanceTuningManager::AddObserver(Observer* o) {
  observers_.AddObserver(o);
}

void UserPerformanceTuningManager::RemoveObserver(Observer* o) {
  observers_.RemoveObserver(o);
}

bool UserPerformanceTuningManager::DeviceHasBattery() const {
  return has_battery_;
}

void UserPerformanceTuningManager::SetTemporaryBatterySaverDisabledForSession(
    bool disabled) {
  // Setting the temporary mode to its current state is a no-op.
  if (battery_saver_mode_disabled_for_session_ == disabled)
    return;

  battery_saver_mode_disabled_for_session_ = disabled;
  UpdateBatterySaverModeState();
}

bool UserPerformanceTuningManager::IsBatterySaverModeDisabledForSession()
    const {
  return battery_saver_mode_disabled_for_session_;
}

bool UserPerformanceTuningManager::IsHighEfficiencyModeActive() const {
  return pref_change_registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
}

bool UserPerformanceTuningManager::IsBatterySaverActive() const {
  return battery_saver_mode_enabled_;
}

bool UserPerformanceTuningManager::IsUsingBatteryPower() const {
  return on_battery_power_;
}

base::Time UserPerformanceTuningManager::GetLastBatteryUsageTimestamp() const {
  return pref_change_registrar_.prefs()->GetTime(
      performance_manager::user_tuning::prefs::kLastBatteryUseTimestamp);
}

int UserPerformanceTuningManager::SampledBatteryPercentage() const {
  return battery_percentage_;
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
    NotifyMemoryMetricsRefreshed() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        // Hitting this CHECK would mean this task is running after
        // PostMainMessageLoopRun, which shouldn't happen.
        CHECK(g_user_performance_tuning_manager);
        GetInstance()->NotifyMemoryMetricsRefreshed();
      }));
}

UserPerformanceTuningManager::UserPerformanceTuningManager(
    PrefService* local_state,
    std::unique_ptr<UserPerformanceTuningNotifier> notifier,
    std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate,
    std::unique_ptr<HighEfficiencyModeToggleDelegate>
        high_efficiency_mode_toggle_delegate)
    : frame_throttling_delegate_(
          frame_throttling_delegate
              ? std::move(frame_throttling_delegate)
              : std::make_unique<FrameThrottlingDelegateImpl>()),
      high_efficiency_mode_toggle_delegate_(
          high_efficiency_mode_toggle_delegate
              ? std::move(high_efficiency_mode_toggle_delegate)
              : std::make_unique<HighEfficiencyModeToggleDelegateImpl>()) {
  DCHECK(!g_user_performance_tuning_manager);
  g_user_performance_tuning_manager = this;

  if (notifier) {
    performance_manager::PerformanceManager::PassToGraph(FROM_HERE,
                                                         std::move(notifier));
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable)) {
    // If the HEM pref is still the default (it wasn't configured by the user),
    // look up what that default value should be in Finch and set it here.
    // This is called in PostCreateThreads, which ensures the pref is in the
    // correct state when views are created.
    const PrefService::Preference* pref = local_state->FindPreference(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
    if (pref->IsDefaultValue()) {
      local_state->SetDefaultPrefValue(
          performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
          base::Value(
              performance_manager::features::kHighEfficiencyModeDefaultState
                  .Get()));
    }
  }

  pref_change_registrar_.Init(local_state);
}

void UserPerformanceTuningManager::Start() {
  was_started_ = true;

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable)) {
    pref_change_registrar_.Add(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
        base::BindRepeating(
            &UserPerformanceTuningManager::OnHighEfficiencyModePrefChanged,
            base::Unretained(this)));
    // Make sure the initial state of the pref is passed on to the policy.
    OnHighEfficiencyModePrefChanged();
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kBatterySaverModeAvailable)) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kForceDeviceHasBattery)) {
      force_has_battery_ = true;
      has_battery_ = true;
    }

    pref_change_registrar_.Add(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        base::BindRepeating(
            &UserPerformanceTuningManager::OnBatterySaverModePrefChanged,
            base::Unretained(this)));

    on_battery_power_ =
        base::PowerMonitor::AddPowerStateObserverAndReturnOnBatteryState(this);

    base::BatteryStateSampler* battery_state_sampler =
        base::BatteryStateSampler::Get();
    // Some platforms don't have a battery sampler, treat them as if they had no
    // battery at all.
    if (battery_state_sampler) {
      battery_state_sampler_obs_.Observe(battery_state_sampler);
    }

    OnBatterySaverModePrefChanged();
  }
}

void UserPerformanceTuningManager::OnHighEfficiencyModePrefChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
  high_efficiency_mode_toggle_delegate_->ToggleHighEfficiencyMode(enabled);
}

void UserPerformanceTuningManager::OnBatterySaverModePrefChanged() {
  battery_saver_mode_disabled_for_session_ = false;
  UpdateBatterySaverModeState();
}

void UserPerformanceTuningManager::UpdateBatterySaverModeState() {
  DCHECK(was_started_);

  using BatterySaverModeState =
      performance_manager::user_tuning::prefs::BatterySaverModeState;
  performance_manager::user_tuning::prefs::BatterySaverModeState state =
      performance_manager::user_tuning::prefs::GetCurrentBatterySaverModeState(
          pref_change_registrar_.prefs());

  bool previously_enabled = battery_saver_mode_enabled_;

  battery_saver_mode_enabled_ = false;

  if (!battery_saver_mode_disabled_for_session_) {
    switch (state) {
      case BatterySaverModeState::kEnabled:
        battery_saver_mode_enabled_ = true;
        break;
      case BatterySaverModeState::kEnabledOnBattery:
        battery_saver_mode_enabled_ = on_battery_power_;
        break;
      case BatterySaverModeState::kEnabledBelowThreshold:
        battery_saver_mode_enabled_ =
            on_battery_power_ && is_below_low_battery_threshold_;
        break;
      default:
        battery_saver_mode_enabled_ = false;
        break;
    }
  }

  // Don't change throttling or notify observers if the mode didn't change.
  if (previously_enabled == battery_saver_mode_enabled_)
    return;

  if (battery_saver_mode_enabled_) {
    frame_throttling_delegate_->StartThrottlingAllFrameSinks();
  } else {
    frame_throttling_delegate_->StopThrottlingAllFrameSinks();
  }

  for (auto& obs : observers_) {
    obs.OnBatterySaverModeChanged(battery_saver_mode_enabled_);
  }
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

void UserPerformanceTuningManager::OnPowerStateChange(bool on_battery_power) {
  on_battery_power_ = on_battery_power;

  // Plugging in the device unsets the temporary disable BSM flag
  if (!on_battery_power) {
    battery_saver_mode_disabled_for_session_ = false;
  }

  for (auto& obs : observers_) {
    obs.OnExternalPowerConnectedChanged(on_battery_power);
  }

  UpdateBatterySaverModeState();
}

void UserPerformanceTuningManager::OnBatteryStateSampled(
    const absl::optional<base::BatteryLevelProvider::BatteryState>&
        battery_state) {
  if (!battery_state)
    return;

  bool had_battery = has_battery_;
  has_battery_ = force_has_battery_ || battery_state->battery_count > 0;

  // If the "has battery" state changed, notify observers.
  if (had_battery != has_battery_) {
    for (auto& obs : observers_) {
      obs.OnDeviceHasBatteryChanged(has_battery_);
    }
  }

  // Log the unplugged battery usage to local pref if the previous value is more
  // than a day old.
  if (has_battery_ && !battery_state->is_external_power_connected &&
      (base::Time::Now() - GetLastBatteryUsageTimestamp() >
       kBatteryUsageWriteFrequency)) {
    pref_change_registrar_.prefs()->SetTime(
        performance_manager::user_tuning::prefs::kLastBatteryUseTimestamp,
        base::Time::Now());
  }

  if (!battery_state->current_capacity ||
      !battery_state->full_charged_capacity) {
    // This should only happen if there are no batteries connected, or multiple
    // batteries connected (in which case their units may not match so they
    // don't report a charge). We're not under the threshold for any battery.
    DCHECK_NE(1, battery_state->battery_count);

    is_below_low_battery_threshold_ = false;
    return;
  }

  battery_percentage_ = *(battery_state->full_charged_capacity) > 0
                            ? *(battery_state->current_capacity) * 100 /
                                  *(battery_state->full_charged_capacity)
                            : 100;

  bool was_below_threshold = is_below_low_battery_threshold_;

  // A battery is below the threshold if it's under 20% charge. On some
  // platforms, we adjust the threshold by a value specified in Finch to account
  // for the displayed battery level being artificially lower than the actual
  // level. See
  // `power_manager::BatteryPercentageConverter::ConvertActualToDisplay`.
  uint64_t adjusted_low_battery_threshold =
      kLowBatteryThresholdPercent +
      performance_manager::features::
          kBatterySaverModeThresholdAdjustmentForDisplayLevel.Get();
  is_below_low_battery_threshold_ =
      battery_percentage_ < static_cast<int>(adjusted_low_battery_threshold);

  if (is_below_low_battery_threshold_ && !was_below_threshold) {
    for (auto& obs : observers_) {
      obs.OnBatteryThresholdReached();
    }
  }

  UpdateBatterySaverModeState();
}

void UserPerformanceTuningManager::DiscardPageForTesting(
    content::WebContents* web_contents) {
  base::RunLoop run_loop;
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             base::WeakPtr<performance_manager::PageNode> page_node,
             performance_manager::Graph* graph) {
            if (page_node) {
              performance_manager::policies::PageDiscardingHelper::GetFromGraph(
                  graph)
                  ->ImmediatelyDiscardSpecificPage(page_node.get());
              quit_closure.Run();
            }
          },
          run_loop.QuitClosure(),
          performance_manager::PerformanceManager::
              GetPrimaryPageNodeForWebContents(web_contents)));
  run_loop.Run();
}

}  // namespace performance_manager::user_tuning
