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
#include "base/power_monitor/battery_state_sampler.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
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

constexpr base::TimeDelta kBatteryUsageWriteFrequency = base::Days(1);

// On certain platforms (ChromeOS), the battery level displayed to the user is
// artificially lower than the actual battery level. Unfortunately, the battery
// level that Battery Saver Mode looks at is the "actual" level, so users on
// that platform may see Battery Saver Mode trigger at say 17% rather than the
// "advertised" 20%. This parameter allows us to heuristically tweak the
// threshold on those platforms, by being added to the 20% threshold value (so
// setting this parameter to 3 would result in battery saver being activated at
// 23% actual battery level).
#if BUILDFLAG(IS_CHROMEOS_ASH)

// On ChromeOS, the adjustment generally seems to be around 3%, sometimes 2%. We
// choose 3% because it gets us close enough, or overestimates (which is better
// than underestimating in this instance).
constexpr int kBatterySaverModeThresholdAdjustmentForDisplayLevel = 3;
#else
constexpr int kBatterySaverModeThresholdAdjustmentForDisplayLevel = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

class DesktopBatterySaverProvider
    : public UserPerformanceTuningManager::BatterySaverProvider,
      public base::PowerStateObserver,
      public base::BatteryStateSampler::Observer {
 public:
  DesktopBatterySaverProvider(UserPerformanceTuningManager* manager,
                              PrefService* local_state)
      : manager_(manager) {
    CHECK(manager_);

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(
            UserPerformanceTuningManager::kForceDeviceHasBatterySwitch)) {
      force_has_battery_ = true;
      has_battery_ = true;
    }

    pref_change_registrar_.Init(local_state);

    pref_change_registrar_.Add(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        base::BindRepeating(
            &DesktopBatterySaverProvider::OnBatterySaverModePrefChanged,
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

  ~DesktopBatterySaverProvider() override {
    base::PowerMonitor::RemovePowerStateObserver(this);
  }

  // BatterySaverProvider:
  bool DeviceHasBattery() const override { return has_battery_; }
  bool IsBatterySaverActive() const override {
    return battery_saver_mode_enabled_;
  }
  bool IsUsingBatteryPower() const override { return on_battery_power_; }
  base::Time GetLastBatteryUsageTimestamp() const override {
    return pref_change_registrar_.prefs()->GetTime(
        performance_manager::user_tuning::prefs::kLastBatteryUseTimestamp);
  }
  int SampledBatteryPercentage() const override { return battery_percentage_; }
  void SetTemporaryBatterySaverDisabledForSession(bool disabled) override {
    // Setting the temporary mode to its current state is a no-op.
    if (battery_saver_mode_disabled_for_session_ == disabled) {
      return;
    }

    battery_saver_mode_disabled_for_session_ = disabled;
    UpdateBatterySaverModeState();
  }

  bool IsBatterySaverModeDisabledForSession() const override {
    return battery_saver_mode_disabled_for_session_;
  }

 private:
  void OnBatterySaverModePrefChanged() {
    battery_saver_mode_disabled_for_session_ = false;
    UpdateBatterySaverModeState();
  }

  void UpdateBatterySaverModeState() {
    using BatterySaverModeState =
        performance_manager::user_tuning::prefs::BatterySaverModeState;
    BatterySaverModeState state = performance_manager::user_tuning::prefs::
        GetCurrentBatterySaverModeState(pref_change_registrar_.prefs());

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
    if (previously_enabled == battery_saver_mode_enabled_) {
      return;
    }

    manager_->NotifyOnBatterySaverModeChanged(battery_saver_mode_enabled_);
  }

  // base::PowerStateObserver:
  void OnPowerStateChange(bool on_battery_power) override {
    on_battery_power_ = on_battery_power;

    // Plugging in the device unsets the temporary disable BSM flag
    if (!on_battery_power) {
      battery_saver_mode_disabled_for_session_ = false;
    }

    manager_->NotifyOnExternalPowerConnectedChanged(on_battery_power);

    UpdateBatterySaverModeState();
  }

  // base::BatteryStateSampler::Observer:
  void OnBatteryStateSampled(
      const absl::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state) override {
    if (!battery_state) {
      return;
    }

    bool had_battery = has_battery_;
    has_battery_ = force_has_battery_ || battery_state->battery_count > 0;

    // If the "has battery" state changed, notify observers.
    if (had_battery != has_battery_) {
      manager_->NotifyOnDeviceHasBatteryChanged(has_battery_);
    }

    // Log the unplugged battery usage to local pref if the previous value is
    // more than a day old.
    if (has_battery_ && !battery_state->is_external_power_connected &&
        (base::Time::Now() - GetLastBatteryUsageTimestamp() >
         kBatteryUsageWriteFrequency)) {
      pref_change_registrar_.prefs()->SetTime(
          performance_manager::user_tuning::prefs::kLastBatteryUseTimestamp,
          base::Time::Now());
    }

    if (!battery_state->current_capacity ||
        !battery_state->full_charged_capacity) {
      // This should only happen if there are no batteries connected, or
      // multiple batteries connected (in which case their units may not match
      // so they don't report a charge). We're not under the threshold for any
      // battery.
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
    // platforms, we adjust the threshold by a value specified in Finch to
    // account for the displayed battery level being artificially lower than the
    // actual level. See
    // `power_manager::BatteryPercentageConverter::ConvertActualToDisplay`.
    uint64_t adjusted_low_battery_threshold =
        UserPerformanceTuningManager::kLowBatteryThresholdPercent +
        kBatterySaverModeThresholdAdjustmentForDisplayLevel;
    is_below_low_battery_threshold_ =
        battery_percentage_ < static_cast<int>(adjusted_low_battery_threshold);

    if (is_below_low_battery_threshold_ && !was_below_threshold) {
      manager_->NotifyOnBatteryThresholdReached();
    }

    UpdateBatterySaverModeState();
  }

  bool battery_saver_mode_enabled_ = false;
  bool battery_saver_mode_disabled_for_session_ = false;

  bool has_battery_ = false;
  bool force_has_battery_ = false;
  bool on_battery_power_ = false;
  bool is_below_low_battery_threshold_ = false;
  int battery_percentage_ = -1;

  base::ScopedObservation<base::BatteryStateSampler,
                          base::BatteryStateSampler::Observer>
      battery_state_sampler_obs_{this};
  PrefChangeRegistrar pref_change_registrar_;

  raw_ptr<UserPerformanceTuningManager> manager_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeOSBatterySaverProvider
    : public UserPerformanceTuningManager::BatterySaverProvider,
      public chromeos::PowerManagerClient::Observer {
 public:
  explicit ChromeOSBatterySaverProvider(UserPerformanceTuningManager* manager)
      : manager_(manager) {
    CHECK(manager_);

    chromeos::PowerManagerClient* client = chromeos::PowerManagerClient::Get();
    CHECK(client);

    power_manager_client_observer_.Observe(client);
    client->GetBatterySaverModeState(base::BindOnce(
        &ChromeOSBatterySaverProvider::OnInitialBatterySaverModeObtained,
        weak_ptr_factory_.GetWeakPtr()));
  }

  ~ChromeOSBatterySaverProvider() override = default;

  void OnInitialBatterySaverModeObtained(
      absl::optional<power_manager::BatterySaverModeState> state) {
    if (state) {
      BatterySaverModeStateChanged(*state);
    }
  }

  // chromeos::PowerManagerClient::Observer:
  void BatterySaverModeStateChanged(
      const power_manager::BatterySaverModeState& state) override {
    if (!state.has_enabled() || enabled_ == state.enabled()) {
      return;
    }

    enabled_ = state.enabled();

    manager_->NotifyOnBatterySaverModeChanged(enabled_);
  }

  // BatterySaverProvider:
  bool DeviceHasBattery() const override { return false; }
  bool IsBatterySaverActive() const override { return enabled_; }
  bool IsUsingBatteryPower() const override { return false; }
  base::Time GetLastBatteryUsageTimestamp() const override {
    return base::Time();
  }
  int SampledBatteryPercentage() const override { return -1; }
  void SetTemporaryBatterySaverDisabledForSession(bool disabled) override {
    NOTREACHED();
    // No-op when BSM is controlled by the OS
  }
  bool IsBatterySaverModeDisabledForSession() const override { return false; }

 private:
  bool enabled_ = false;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_{this};

  raw_ptr<UserPerformanceTuningManager> manager_;

  base::WeakPtrFactory<ChromeOSBatterySaverProvider> weak_ptr_factory_{this};
};
#endif

const uint64_t UserPerformanceTuningManager::kLowBatteryThresholdPercent = 20;

const char UserPerformanceTuningManager::kForceDeviceHasBatterySwitch[] =
    "force-device-has-battery";

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

bool UserPerformanceTuningManager::DeviceHasBattery() const {
  return battery_saver_provider_ && battery_saver_provider_->DeviceHasBattery();
}

void UserPerformanceTuningManager::SetTemporaryBatterySaverDisabledForSession(
    bool disabled) {
  CHECK(battery_saver_provider_);
  battery_saver_provider_->SetTemporaryBatterySaverDisabledForSession(disabled);
}

bool UserPerformanceTuningManager::IsBatterySaverModeDisabledForSession()
    const {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsBatterySaverModeDisabledForSession();
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

bool UserPerformanceTuningManager::IsBatterySaverActive() const {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsBatterySaverActive();
}

bool UserPerformanceTuningManager::IsUsingBatteryPower() const {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsUsingBatteryPower();
}

base::Time UserPerformanceTuningManager::GetLastBatteryUsageTimestamp() const {
  return battery_saver_provider_
             ? battery_saver_provider_->GetLastBatteryUsageTimestamp()
             : base::Time();
}

int UserPerformanceTuningManager::SampledBatteryPercentage() const {
  return battery_saver_provider_
             ? battery_saver_provider_->SampledBatteryPercentage()
             : -1;
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

void UserPerformanceTuningManager::NotifyOnBatterySaverModeChanged(
    bool battery_saver_mode_enabled) {
  if (battery_saver_mode_enabled) {
    frame_throttling_delegate_->StartThrottlingAllFrameSinks();
  } else {
    frame_throttling_delegate_->StopThrottlingAllFrameSinks();
  }

  for (auto& obs : observers_) {
    obs.OnBatterySaverModeChanged(battery_saver_mode_enabled);
  }
}
void UserPerformanceTuningManager::NotifyOnExternalPowerConnectedChanged(
    bool on_battery_power) {
  for (auto& obs : observers_) {
    obs.OnExternalPowerConnectedChanged(on_battery_power);
  }
}

void UserPerformanceTuningManager::NotifyOnDeviceHasBatteryChanged(
    bool has_battery) {
  for (auto& obs : observers_) {
    obs.OnDeviceHasBatteryChanged(has_battery);
  }
}
void UserPerformanceTuningManager::NotifyOnBatteryThresholdReached() {
  for (auto& obs : observers_) {
    obs.OnBatteryThresholdReached();
  }
}

UserPerformanceTuningManager::UserPerformanceTuningManager(
    PrefService* local_state,
    std::unique_ptr<UserPerformanceTuningNotifier> notifier,
    std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate,
    std::unique_ptr<HighEfficiencyModeDelegate> high_efficiency_mode_delegate)
    : frame_throttling_delegate_(
          frame_throttling_delegate
              ? std::move(frame_throttling_delegate)
              : std::make_unique<FrameThrottlingDelegateImpl>()),
      high_efficiency_mode_delegate_(
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsBatterySaverAvailable()) {
    battery_saver_provider_ =
        std::make_unique<ChromeOSBatterySaverProvider>(this);
  } else {
    battery_saver_provider_ = std::make_unique<DesktopBatterySaverProvider>(
        this, pref_change_registrar_.prefs());
  }
#else
  battery_saver_provider_ = std::make_unique<DesktopBatterySaverProvider>(
      this, pref_change_registrar_.prefs());
#endif
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
