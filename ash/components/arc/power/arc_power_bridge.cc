// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/power/arc_power_bridge.h"

#include <algorithm>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/display/manager/display_configurator.h"

namespace arc {
namespace {

// Delay for notifying Android about screen brightness changes, added in
// order to prevent spammy brightness updates.
constexpr base::TimeDelta kNotifyBrightnessDelay = base::Milliseconds(200);

}  // namespace

using mojom::IdleState;

// static
ArcPowerBridgeFactory* ArcPowerBridgeFactory::GetInstance() {
  return base::Singleton<ArcPowerBridgeFactory>::get();
}

// WakeLockRequestor requests a wake lock from the device service in response
// to wake lock requests of a given type from Android. A count is kept of
// outstanding Android requests so that only a single actual wake lock is used.
class ArcPowerBridge::WakeLockRequestor {
 public:
  WakeLockRequestor(device::mojom::WakeLockType type,
                    device::mojom::WakeLockProvider* provider)
      : type_(type), provider_(provider) {}

  WakeLockRequestor(const WakeLockRequestor&) = delete;
  WakeLockRequestor& operator=(const WakeLockRequestor&) = delete;

  ~WakeLockRequestor() = default;

  // Increments the number of outstanding requests from Android and requests a
  // wake lock from the device service if this is the only request.
  void AddRequest() {
    num_android_requests_++;
    if (num_android_requests_ > 1)
      return;

    // Initialize |wake_lock_| if this is the first time we're using it.
    if (!wake_lock_) {
      provider_->GetWakeLockWithoutContext(
          type_, device::mojom::WakeLockReason::kOther, "ARC",
          wake_lock_.BindNewPipeAndPassReceiver());
    }

    wake_lock_->RequestWakeLock();
  }

  // Decrements the number of outstanding Android requests. Cancels the device
  // service wake lock when the request count hits zero.
  void RemoveRequest() {
    DCHECK_GT(num_android_requests_, 0);
    num_android_requests_--;
    if (num_android_requests_ >= 1)
      return;

    DCHECK(wake_lock_);
    wake_lock_->CancelWakeLock();
  }

  // Runs the message loop until replies have been received for all pending
  // requests on |wake_lock_|.
  void FlushForTesting() {
    if (wake_lock_)
      wake_lock_.FlushForTesting();
  }

 private:
  // Type of wake lock to request.
  const device::mojom::WakeLockType type_;

  // The WakeLockProvider implementation we use to request WakeLocks. Not owned.
  const raw_ptr<device::mojom::WakeLockProvider> provider_;

  // Number of outstanding Android requests.
  int num_android_requests_ = 0;

  // Lazily initialized in response to first request.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
};

// static
ArcPowerBridge* ArcPowerBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPowerBridgeFactory::GetForBrowserContext(context);
}

// static
ArcPowerBridge* ArcPowerBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcPowerBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcPowerBridge::ArcPowerBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->power()->SetHost(this);
  arc_bridge_service_->power()->AddObserver(this);
}

ArcPowerBridge::~ArcPowerBridge() {
  for (auto& observer : observer_list_) {
    observer.OnWillDestroyArcPowerBridge();
  }
  arc_bridge_service_->power()->RemoveObserver(this);
  arc_bridge_service_->power()->SetHost(nullptr);
}

void ArcPowerBridge::DisableAndroidIdleControl() {
  android_idle_control_disabled_ = true;
}

void ArcPowerBridge::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcPowerBridge::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcPowerBridge::SetUserIdHash(const std::string& user_id_hash) {
  user_id_hash_ = user_id_hash;
}

bool ArcPowerBridge::TriggerNotifyBrightnessTimerForTesting() {
  if (!notify_brightness_timer_.IsRunning())
    return false;
  notify_brightness_timer_.FireNow();
  return true;
}

void ArcPowerBridge::FlushWakeLocksForTesting() {
  for (const auto& it : wake_lock_requestors_)
    it.second->FlushForTesting();
}

void ArcPowerBridge::OnConnectionReady() {
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->display_configurator()->AddObserver(this);
    // Whether display is on is the same signal as whether Android is interactive
    // or not.
    IsDisplayOn(base::BindOnce(
        [](base::WeakPtr<ArcPowerBridge> power_bridge,
           ArcBridgeService* bridge_service, bool display_on) {
          if (!bridge_service) {
            return;
          }
          power_bridge->NotifyAndroidIdleState(
              bridge_service,
              display_on ? IdleState::ACTIVE : IdleState::INACTIVE);
        },
        weak_ptr_factory_.GetWeakPtr(), arc_bridge_service_));
  }
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      base::BindOnce(&ArcPowerBridge::OnGetScreenBrightnessPercent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcPowerBridge::OnConnectionClosed() {
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->display_configurator()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  wake_lock_requestors_.clear();
}

void ArcPowerBridge::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  is_suspending_ = true;
  mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->power(), Suspend);
  if (!power_instance) {
    LOG(WARNING) << "ArcPower: ignoring request due to no bridge.";
    return;
  }

  VLOG(1) << "ArcPower: will request android suspend.";
  auto token = base::UnguessableToken::Create();
  chromeos::PowerManagerClient::Get()->BlockSuspend(token, "ArcPowerBridge");
  power_instance->Suspend(base::BindOnce(&ArcPowerBridge::OnAndroidSuspendReady,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         token));
  ash::PatchPanelClient::Get()->NotifyAndroidInteractiveState(false);
}

void ArcPowerBridge::OnAndroidSuspendReady(base::UnguessableToken token) {
  // For the ARCVM case, we only want to suspend the VM if a suspend is still
  // underway ie. if SuspendImminent was observed without a subsequent
  // SuspendDone. Otherwise, skip suspending the VM but still call
  // UnblockSuspend to fulfill the contract and to align with ARC container's
  // behavior.
  if (arc::IsArcVmEnabled() && is_suspending_) {
    vm_tools::concierge::SuspendVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    ash::ConciergeClient::Get()->SuspendVm(
        request, base::BindOnce(&ArcPowerBridge::OnConciergeSuspendVmResponse,
                                weak_ptr_factory_.GetWeakPtr(), token));
    return;
  }

  chromeos::PowerManagerClient::Get()->UnblockSuspend(token);
}

void ArcPowerBridge::OnConciergeSuspendVmResponse(
    base::UnguessableToken token,
    std::optional<vm_tools::concierge::SuspendVmResponse> reply) {
  if (!reply.has_value())
    LOG(ERROR) << "Failed to suspend arcvm, no reply received.";
  else if (!reply.value().success())
    LOG(ERROR) << "Failed to suspend arcvm: " << reply.value().failure_reason();
  chromeos::PowerManagerClient::Get()->UnblockSuspend(token);
}

void ArcPowerBridge::SuspendDone(base::TimeDelta sleep_duration) {
  VLOG(1) << "ArcPower: Host waking up.";
  is_suspending_ = false;
  if (arc::IsArcVmEnabled()) {
    vm_tools::concierge::ResumeVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    ash::ConciergeClient::Get()->ResumeVm(
        request, base::BindOnce(&ArcPowerBridge::OnConciergeResumeVmResponse,
                                weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  DispatchAndroidResume();
}

void ArcPowerBridge::OnConciergeResumeVmResponse(
    std::optional<vm_tools::concierge::ResumeVmResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to resume arcvm, no reply received.";
    return;
  }
  if (!reply.value().success()) {
    LOG(ERROR) << "Failed to resume arcvm: " << reply.value().failure_reason();
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnVmResumed();
  }
  DispatchAndroidResume();
}

void ArcPowerBridge::DispatchAndroidResume() {
  if (android_idle_control_disabled_)
    return;

  mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->power(), Resume);
  if (!power_instance) {
    LOG(WARNING) << "ArcPower: Ignoring ARC resume due to no bridge.";
    return;
  }
  VLOG(1) << "ArcPower: Requesting Android resume.";
  power_instance->Resume();
}

void ArcPowerBridge::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  const base::TimeTicks now = base::TimeTicks::Now();
  if (last_brightness_changed_time_.is_null() ||
      (now - last_brightness_changed_time_) >= kNotifyBrightnessDelay) {
    UpdateAndroidScreenBrightness(change.percent());
    notify_brightness_timer_.Stop();
  } else {
    notify_brightness_timer_.Start(
        FROM_HERE, kNotifyBrightnessDelay,
        base::BindOnce(&ArcPowerBridge::UpdateAndroidScreenBrightness,
                       weak_ptr_factory_.GetWeakPtr(), change.percent()));
  }
  last_brightness_changed_time_ = now;
}

void ArcPowerBridge::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  // ARCVM doesn't use this message, since it gets the corresponding
  // information from crosvm's goldfish battery device.
  if (arc::IsArcVmEnabled()) {
    return;
  }

  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->power(), PowerSupplyInfoChanged);
  if (!power_instance)
    return;

  power_instance->PowerSupplyInfoChanged();
}

void ArcPowerBridge::BatterySaverModeStateChanged(
    const power_manager::BatterySaverModeState& state) {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->power(), OnBatterySaverModeStateChanged);
  if (!power_instance) {
    return;
  }

  mojom::BatterySaverModeStatePtr mojo_state =
      mojom::BatterySaverModeState::New();
  mojo_state->active = state.enabled();
  power_instance->OnBatterySaverModeStateChanged(std::move(mojo_state));
}

void ArcPowerBridge::OnPowerStateChanged(
    chromeos::DisplayPowerState power_state) {
  if (android_idle_control_disabled_)
    return;

  NotifyAndroidIdleState(arc_bridge_service_,
                         power_state != chromeos::DISPLAY_POWER_ALL_OFF
                             ? IdleState::ACTIVE
                             : IdleState::INACTIVE);
}

void ArcPowerBridge::NotifyAndroidIdleState(ArcBridgeService* bridge,
                                            IdleState state) {
  if (!bridge) {
    return;
  }
  if (state != IdleState::ACTIVE && is_suspending_) {
    LOG(WARNING) << "Suspend is in progress, avoiding display disable";
    return;
  }

  auto* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge->power(), SetIdleState);
  if (power_instance) {
    VLOG(1) << "ArcPower: SetIdleState to " << state;
    power_instance->SetIdleState(state);
  } else if ((power_instance = ARC_GET_INSTANCE_FOR_METHOD(
                  bridge->power(), SetInteractiveDeprecated))) {
    VLOG(1) << "ArcPower: SetInteractiveDeprecated to "
            << (state == IdleState::ACTIVE);
    power_instance->SetInteractiveDeprecated(state == IdleState::ACTIVE);
  } else {
    LOG(WARNING) << "ArcPower: Avoiding display change due to no bridge.";
    return;
  }

  // Display power state is the same signal as Android Idle state. When
  // power state changes, notify Android interactive state change as well.
  ash::PatchPanelClient::Get()->NotifyAndroidInteractiveState(
      state == IdleState::ACTIVE);

  for (auto& observer : observer_list_) {
    observer.OnAndroidIdleStateChange(state);
  }
}

void ArcPowerBridge::OnAcquireDisplayWakeLock(mojom::DisplayWakeLockType type) {
  switch (type) {
    case mojom::DisplayWakeLockType::BRIGHT:
      GetWakeLockRequestor(device::mojom::WakeLockType::kPreventDisplaySleep)
          ->AddRequest();
      break;
    case mojom::DisplayWakeLockType::DIM:
      GetWakeLockRequestor(
          device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming)
          ->AddRequest();
      break;
    default:
      LOG(WARNING) << "Tried to take invalid wake lock type "
                   << static_cast<int>(type);
      return;
  }
}

void ArcPowerBridge::OnReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) {
  switch (type) {
    case mojom::DisplayWakeLockType::BRIGHT:
      GetWakeLockRequestor(device::mojom::WakeLockType::kPreventDisplaySleep)
          ->RemoveRequest();
      break;
    case mojom::DisplayWakeLockType::DIM:
      GetWakeLockRequestor(
          device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming)
          ->RemoveRequest();
      break;
    default:
      LOG(WARNING) << "Tried to take invalid wake lock type "
                   << static_cast<int>(type);
      return;
  }
}

void ArcPowerBridge::IsDisplayOn(IsDisplayOnCallback callback) {
  bool is_display_on = false;
  // TODO(mash): Support this functionality without ash::Shell access in Chrome.
  if (ash::Shell::HasInstance())
    is_display_on = ash::Shell::Get()->display_configurator()->IsDisplayOn();
  std::move(callback).Run(is_display_on);
}

void ArcPowerBridge::OnScreenBrightnessUpdateRequest(double percent) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  request.set_transition(
      power_manager::SetBacklightBrightnessRequest_Transition_FAST);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
}

ArcPowerBridge::WakeLockRequestor* ArcPowerBridge::GetWakeLockRequestor(
    device::mojom::WakeLockType type) {
  auto it = wake_lock_requestors_.find(type);
  if (it != wake_lock_requestors_.end())
    return it->second.get();

  if (!wake_lock_provider_) {
    content::GetDeviceService().BindWakeLockProvider(
        wake_lock_provider_.BindNewPipeAndPassReceiver());
  }

  it = wake_lock_requestors_
           .emplace(type, std::make_unique<WakeLockRequestor>(
                              type, wake_lock_provider_.get()))
           .first;
  return it->second.get();
}

void ArcPowerBridge::OnGetScreenBrightnessPercent(
    std::optional<double> percent) {
  if (!percent.has_value()) {
    LOG(ERROR)
        << "PowerManagerClient::GetScreenBrightnessPercent reports an error";
    return;
  }
  UpdateAndroidScreenBrightness(percent.value());
}

void ArcPowerBridge::OnWakefulnessChanged(mojom::WakefulnessMode mode) {
  for (auto& observer : observer_list_)
    observer.OnWakefulnessChanged(mode);
}

void ArcPowerBridge::OnPreAnr(mojom::AnrType type) {
  base::UmaHistogramEnumeration("Arc.Anr.PreNotified", type);
  for (auto& observer : observer_list_)
    observer.OnPreAnr(type);
}

void ArcPowerBridge::OnAnrRecoveryFailed(::arc::mojom::AnrType type) {
  base::UmaHistogramEnumeration("Arc.Anr.RecoveryFailed", type);
}

void ArcPowerBridge::GetBatterySaverModeState(
    GetBatterySaverModeStateCallback callback) {
  chromeos::PowerManagerClient::Get()->GetBatterySaverModeState(
      base::BindOnce(&ArcPowerBridge::OnBatterySaverModeStateReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcPowerBridge::OnBatterySaverModeStateReceived(
    GetBatterySaverModeStateCallback callback,
    std::optional<power_manager::BatterySaverModeState> state) {
  mojom::BatterySaverModeStatePtr mojo_state =
      mojom::BatterySaverModeState::New();
  if (state.has_value()) {
    mojo_state->active = state->enabled();
  } else {
    LOG(ERROR)
        << "PowerManagerClient::GetBatterySaverModeState reports an error";
  }
  std::move(callback).Run(std::move(mojo_state));
}

void ArcPowerBridge::UpdateAndroidScreenBrightness(double percent) {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->power(), UpdateScreenBrightnessSettings);
  if (!power_instance)
    return;
  power_instance->UpdateScreenBrightnessSettings(percent);
}

// static
void ArcPowerBridge::EnsureFactoryBuilt() {
  ArcPowerBridgeFactory::GetInstance();
}

}  // namespace arc
