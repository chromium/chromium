// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump.h"
#include "base/notreached.h"
#include "base/power_monitor/battery_state_sampler.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "components/performance_manager/freezing/freezing_policy.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/frame_rate_throttling.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#endif

namespace performance_manager::user_tuning {
namespace {

BatterySaverModeManager* g_battery_saver_mode_manager = nullptr;

constexpr base::TimeDelta kBatteryUsageWriteFrequency = base::Days(1);

using BatterySaverModeState =
    performance_manager::user_tuning::prefs::BatterySaverModeState;

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
    : public performance_manager::user_tuning::BatterySaverModeManager::
          FrameThrottlingDelegate {
 public:
  void StartThrottlingAllFrameSinks() override {
    content::StartThrottlingAllFrameSinks(base::Hertz(30));
  }

  void StopThrottlingAllFrameSinks() override {
    content::StopThrottlingAllFrameSinks();
  }

  ~FrameThrottlingDelegateImpl() override = default;
};

class ChildProcessTuningDelegateImpl
    : public BatterySaverModeManager::ChildProcessTuningDelegate,
      public content::RenderProcessHostCreationObserver,
      public content::BrowserChildProcessObserver,
      public content::RenderProcessHostObserver {
 public:
  ~ChildProcessTuningDelegateImpl() override {
    content::BrowserChildProcessObserver::Remove(this);
  }
  ChildProcessTuningDelegateImpl() {
    content::BrowserChildProcessObserver::Add(this);
  }

 private:
  void SetBatterySaverModeForAllChildProcessHosts(bool enabled) override {
    for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
      if (!iter.GetData().GetProcess().IsValid()) {
        continue;
      }

      iter.GetHost()->SetBatterySaverMode(enabled);
    }

    for (content::RenderProcessHost::iterator iter(
             content::RenderProcessHost::AllHostsIterator());
         !iter.IsAtEnd(); iter.Advance()) {
      content::RenderProcessHost* host = iter.GetCurrentValue();

      if (host->IsReady()) {
        host->SetBatterySaverMode(enabled);
      }
    }
    battery_saver_mode_enabled_ = enabled;
  }

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override {
    // The RenderProcessHost can be reused for a new process, sending a new
    // `OnRenderProcessHostCreated` notification. In this case, the RPH is
    // already being observed so no need to observe it again.
    if (!observed_render_process_hosts_.IsObservingSource(host)) {
      observed_render_process_hosts_.AddObservation(host);
    }
  }

  // content::RenderProcessHostObserver:
  void RenderProcessReady(content::RenderProcessHost* host) override {
    // The default state is false, so only do the mojo call if the state should
    // be set to true.
    if (battery_saver_mode_enabled_) {
      host->SetBatterySaverMode(battery_saver_mode_enabled_);
    }
  }

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override {
    // TODO(etiennep): Replace this by a CHECK.
    if (!data.GetProcess().IsValid()) {
      return;
    }
    if (battery_saver_mode_enabled_) {
      content::BrowserChildProcessHost* host =
          content::BrowserChildProcessHost::FromID(data.id);
      if (!host) {
        return;
      }
      host->GetHost()->SetBatterySaverMode(battery_saver_mode_enabled_);
    }
  }

  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override {
    CHECK(observed_render_process_hosts_.IsObservingSource(host));
    observed_render_process_hosts_.RemoveObservation(host);
  }

  bool battery_saver_mode_enabled_ = false;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      observed_render_process_hosts_{this};
};

class FreezingDelegateImpl : public BatterySaverModeManager::FreezingDelegate {
 public:
  FreezingDelegateImpl() = default;
  ~FreezingDelegateImpl() override = default;

  void ToggleFreezingOnBatterySaverMode(bool is_enabled) final {
    PerformanceManagerImpl::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](bool is_enabled, performance_manager::Graph* graph) {
              CHECK_DEREF(graph->GetRegisteredObjectAs<FreezingPolicy>())
                  .ToggleFreezingOnBatterySaverMode(is_enabled);
            },
            is_enabled));
  }
};

}  // namespace

class DesktopBatterySaverProvider
    : public BatterySaverModeManager::BatterySaverProvider,
      public base::PowerStateObserver,
      public base::BatteryStateSampler::Observer {
 public:
  DesktopBatterySaverProvider(BatterySaverModeManager* manager,
                              PrefService* local_state)
      : manager_(manager) {
    CHECK(manager_);

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(
            BatterySaverModeManager::kForceDeviceHasBatterySwitch)) {
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
        base::PowerMonitor::GetInstance()
            ->AddPowerStateObserverAndReturnBatteryPowerStatus(this) ==
        base::PowerStateObserver::BatteryPowerStatus::kBatteryPower;

    base::BatteryStateSampler* battery_state_sampler =
        base::BatteryStateSampler::Get();
    // Some platforms don't have a battery sampler, treat them as if they had no
    // battery at all.
    if (battery_state_sampler) {
      battery_state_sampler_obs_.Observe(battery_state_sampler);
    }

    UpdateBatterySaverModeState();
  }

  ~DesktopBatterySaverProvider() override {
    base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
  }

  // BatterySaverProvider:
  bool DeviceHasBattery() const override { return has_battery_; }
  bool IsBatterySaverModeEnabled() override {
    BatterySaverModeState state = performance_manager::user_tuning::prefs::
        GetCurrentBatterySaverModeState(pref_change_registrar_.prefs());
    return state != BatterySaverModeState::kDisabled;
  }
  bool IsBatterySaverModeManaged() override {
    auto* pref = pref_change_registrar_.prefs()->FindPreference(
        prefs::kBatterySaverModeState);
    return pref->IsManaged();
  }
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
    manager_->NotifyOnBatterySaverModeChanged(
        performance_manager::user_tuning::prefs::
            GetCurrentBatterySaverModeState(pref_change_registrar_.prefs()) !=
        performance_manager::user_tuning::prefs::BatterySaverModeState::
            kDisabled);
  }

  void UpdateBatterySaverModeState() {
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

    manager_->NotifyOnBatterySaverActiveChanged(battery_saver_mode_enabled_);
  }

  // base::PowerStateObserver:
  void OnBatteryPowerStatusChange(base::PowerStateObserver::BatteryPowerStatus
                                      battery_power_status) override {
    on_battery_power_ = (battery_power_status ==
                         PowerStateObserver::BatteryPowerStatus::kBatteryPower);

    // Plugging in the device unsets the temporary disable BSM flag
    if (!on_battery_power_) {
      battery_saver_mode_disabled_for_session_ = false;
    }

    manager_->NotifyOnExternalPowerConnectedChanged(on_battery_power_);

    UpdateBatterySaverModeState();
  }

  // base::BatteryStateSampler::Observer:
  void OnBatteryStateSampled(
      const std::optional<base::BatteryLevelProvider::BatteryState>&
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
        BatterySaverModeManager::kLowBatteryThresholdPercent +
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

  raw_ptr<BatterySaverModeManager> manager_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeOSBatterySaverProvider
    : public BatterySaverModeManager::BatterySaverProvider,
      public chromeos::PowerManagerClient::Observer {
 public:
  explicit ChromeOSBatterySaverProvider(BatterySaverModeManager* manager)
      : manager_(manager) {
    CHECK(manager_);

    chromeos::PowerManagerClient* client = chromeos::PowerManagerClient::Get();
    if (client) {
      power_manager_client_observer_.Observe(client);
      client->GetBatterySaverModeState(base::BindOnce(
          &ChromeOSBatterySaverProvider::OnInitialBatterySaverModeObtained,
          weak_ptr_factory_.GetWeakPtr()));
    } else {
      // We must be in a test that didn't set up PowerManagerClient, so we don't
      // need to listen for updates from it.
      CHECK_IS_TEST();
    }

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(
            BatterySaverModeManager::kForceDeviceHasBatterySwitch)) {
      force_has_battery_ = true;
      has_battery_ = true;
    }
  }

  ~ChromeOSBatterySaverProvider() override = default;

  void OnInitialBatterySaverModeObtained(
      std::optional<power_manager::BatterySaverModeState> state) {
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

    manager_->NotifyOnBatterySaverActiveChanged(enabled_);
  }

  void PowerChanged(
      const power_manager::PowerSupplyProperties& proto) override {
    bool device_has_battery =
        proto.battery_state() !=
        power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
    has_battery_ = force_has_battery_ || device_has_battery;
  }

  // BatterySaverProvider:
  bool DeviceHasBattery() const override { return has_battery_; }
  bool IsBatterySaverModeEnabled() override { return false; }
  bool IsBatterySaverModeManaged() override { return false; }
  bool IsBatterySaverActive() const override { return enabled_; }
  bool IsUsingBatteryPower() const override { return false; }
  base::Time GetLastBatteryUsageTimestamp() const override {
    return base::Time();
  }
  int SampledBatteryPercentage() const override { return -1; }
  void SetTemporaryBatterySaverDisabledForSession(bool disabled) override {
    NOTREACHED_IN_MIGRATION();
    // No-op when BSM is controlled by the OS
  }
  bool IsBatterySaverModeDisabledForSession() const override { return false; }

 private:
  bool enabled_ = false;
  bool has_battery_ = false;
  bool force_has_battery_ = false;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_{this};

  raw_ptr<BatterySaverModeManager> manager_;

  base::WeakPtrFactory<ChromeOSBatterySaverProvider> weak_ptr_factory_{this};
};
#endif

const uint64_t BatterySaverModeManager::kLowBatteryThresholdPercent = 20;

const char BatterySaverModeManager::kForceDeviceHasBatterySwitch[] =
    "force-device-has-battery";

// static
bool BatterySaverModeManager::HasInstance() {
  return g_battery_saver_mode_manager;
}

// static
BatterySaverModeManager* BatterySaverModeManager::GetInstance() {
  DCHECK(g_battery_saver_mode_manager);
  return g_battery_saver_mode_manager;
}

BatterySaverModeManager::~BatterySaverModeManager() {
  DCHECK_EQ(this, g_battery_saver_mode_manager);
  g_battery_saver_mode_manager = nullptr;
}

void BatterySaverModeManager::AddObserver(Observer* o) {
  observers_.AddObserver(o);
}

void BatterySaverModeManager::RemoveObserver(Observer* o) {
  observers_.RemoveObserver(o);
}

bool BatterySaverModeManager::DeviceHasBattery() const {
  return battery_saver_provider_ && battery_saver_provider_->DeviceHasBattery();
}

bool BatterySaverModeManager::IsBatterySaverModeEnabled() {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsBatterySaverModeEnabled();
}

bool BatterySaverModeManager::IsBatterySaverModeManaged() const {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsBatterySaverModeManaged();
}

bool BatterySaverModeManager::IsBatterySaverActive() const {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsBatterySaverActive();
}

bool BatterySaverModeManager::IsUsingBatteryPower() const {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsUsingBatteryPower();
}

base::Time BatterySaverModeManager::GetLastBatteryUsageTimestamp() const {
  return battery_saver_provider_
             ? battery_saver_provider_->GetLastBatteryUsageTimestamp()
             : base::Time();
}

int BatterySaverModeManager::SampledBatteryPercentage() const {
  return battery_saver_provider_
             ? battery_saver_provider_->SampledBatteryPercentage()
             : -1;
}

void BatterySaverModeManager::SetTemporaryBatterySaverDisabledForSession(
    bool disabled) {
  CHECK(battery_saver_provider_);
  battery_saver_provider_->SetTemporaryBatterySaverDisabledForSession(disabled);
}

bool BatterySaverModeManager::IsBatterySaverModeDisabledForSession() const {
  return battery_saver_provider_ &&
         battery_saver_provider_->IsBatterySaverModeDisabledForSession();
}

BatterySaverModeManager::BatterySaverModeManager(
    PrefService* local_state,
    std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate,
    std::unique_ptr<ChildProcessTuningDelegate> child_process_tuning_delegate,
    std::unique_ptr<FreezingDelegate> freezing_delegate)
    : frame_throttling_delegate_(
          frame_throttling_delegate
              ? std::move(frame_throttling_delegate)
              : std::make_unique<FrameThrottlingDelegateImpl>()),
      child_process_tuning_delegate_(
          child_process_tuning_delegate
              ? std::move(child_process_tuning_delegate)
              : std::make_unique<ChildProcessTuningDelegateImpl>()),
      freezing_delegate_(freezing_delegate
                             ? std::move(freezing_delegate)
                             : std::make_unique<FreezingDelegateImpl>()) {
  DCHECK(!g_battery_saver_mode_manager);
  g_battery_saver_mode_manager = this;

  pref_change_registrar_.Init(local_state);
}

void BatterySaverModeManager::Start() {
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

void BatterySaverModeManager::NotifyOnBatterySaverModeChanged(
    bool battery_saver_mode_enabled) {
  for (auto& obs : observers_) {
    obs.OnBatterySaverModeChanged(battery_saver_mode_enabled);
  }
}

void BatterySaverModeManager::NotifyOnBatterySaverActiveChanged(
    bool battery_saver_mode_active) {
  if (battery_saver_mode_active) {
    frame_throttling_delegate_->StartThrottlingAllFrameSinks();
    if (base::FeatureList::IsEnabled(
            ::features::kBatterySaverModeAlignWakeUps)) {
      base::MessagePump::OverrideAlignWakeUpsState(true,
                                                   base::Milliseconds(32));
    }
  } else {
    frame_throttling_delegate_->StopThrottlingAllFrameSinks();
    if (base::FeatureList::IsEnabled(
            ::features::kBatterySaverModeAlignWakeUps)) {
      base::MessagePump::ResetAlignWakeUpsState();
    }
  }

  child_process_tuning_delegate_->SetBatterySaverModeForAllChildProcessHosts(
      battery_saver_mode_active);

  freezing_delegate_->ToggleFreezingOnBatterySaverMode(
      battery_saver_mode_active);

  for (auto& obs : observers_) {
    obs.OnBatterySaverActiveChanged(battery_saver_mode_active);
  }
}

void BatterySaverModeManager::NotifyOnExternalPowerConnectedChanged(
    bool on_battery_power) {
  for (auto& obs : observers_) {
    obs.OnExternalPowerConnectedChanged(on_battery_power);
  }
}

void BatterySaverModeManager::NotifyOnDeviceHasBatteryChanged(
    bool has_battery) {
  for (auto& obs : observers_) {
    obs.OnDeviceHasBatteryChanged(has_battery);
  }
}
void BatterySaverModeManager::NotifyOnBatteryThresholdReached() {
  for (auto& obs : observers_) {
    obs.OnBatteryThresholdReached();
  }
}

}  // namespace performance_manager::user_tuning
