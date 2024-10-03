// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_POWER_ARC_POWER_BRIDGE_H_
#define ASH_COMPONENTS_ARC_POWER_ARC_POWER_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/anr.mojom.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/display/manager/display_configurator.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;
class ArcPowerBridge;  // So we can declare the factory first.

// Singleton factory for ArcPowerBridge.
class ArcPowerBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPowerBridge,
          ArcPowerBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPowerBridgeFactory";

  static ArcPowerBridgeFactory* GetInstance();

 private:
  friend base::DefaultSingletonTraits<ArcPowerBridgeFactory>;
  ArcPowerBridgeFactory() = default;
  ~ArcPowerBridgeFactory() override = default;
};

// ARC Power Client sets power management policy based on requests from
// ARC instances.
class ArcPowerBridge : public KeyedService,
                       public ConnectionObserver<mojom::PowerInstance>,
                       public chromeos::PowerManagerClient::Observer,
                       public display::DisplayConfigurator::Observer,
                       public mojom::PowerHost {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies that wakefulness mode is changed.
    virtual void OnWakefulnessChanged(mojom::WakefulnessMode mode) {}
    virtual void OnPreAnr(mojom::AnrType type) {}

    // Notifies about resume state of the underlying VM (ARCVM-exclusive).
    // ARC Container does not communicate with CrosVM, and it doesn't
    // instantiate services that care about this event.
    virtual void OnVmResumed() {}

    virtual void OnWillDestroyArcPowerBridge() {}

    // Notifies when android idle state is changed.
    virtual void OnAndroidIdleStateChange(::arc::mojom::IdleState state) {}
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPowerBridge* GetForBrowserContext(content::BrowserContext* context);
  static ArcPowerBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcPowerBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);

  ArcPowerBridge(const ArcPowerBridge&) = delete;
  ArcPowerBridge& operator=(const ArcPowerBridge&) = delete;

  ~ArcPowerBridge() override;

  // Disables Android Idle Control logic locally.
  // This is necessary because we are in the process of moving this control
  // to a different class, and need to accommodate both cases.
  // TODO(b/259622742): remove this once the new control is default.
  void DisableAndroidIdleControl();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetUserIdHash(const std::string& user_id_hash);

  // If |notify_brightness_timer_| is set, runs it and returns true. Returns
  // false otherwise.
  [[nodiscard]] bool TriggerNotifyBrightnessTimerForTesting();

  // Runs the message loop until replies have been received for all pending
  // device service requests in |wake_lock_requestors_|.
  void FlushWakeLocksForTesting();

  // ConnectionObserver<mojom::PowerInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // chromeos::PowerManagerClient::Observer overrides.
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void BatterySaverModeStateChanged(
      const power_manager::BatterySaverModeState& state) override;

  // DisplayConfigurator::Observer overrides.
  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override;

  // mojom::PowerHost overrides.
  void OnAcquireDisplayWakeLock(mojom::DisplayWakeLockType type) override;
  void OnReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) override;
  void IsDisplayOn(IsDisplayOnCallback callback) override;
  void OnScreenBrightnessUpdateRequest(double percent) override;
  void OnWakefulnessChanged(mojom::WakefulnessMode mode) override;
  void OnPreAnr(mojom::AnrType type) override;
  void OnAnrRecoveryFailed(::arc::mojom::AnrType type) override;
  void GetBatterySaverModeState(
      GetBatterySaverModeStateCallback callback) override;

  void SetWakeLockProviderForTesting(
      mojo::Remote<device::mojom::WakeLockProvider> provider) {
    wake_lock_provider_ = std::move(provider);
  }

  static void EnsureFactoryBuilt();

  // Notify ARC and patchpanel about change in power state, notification to
  // patchpanel is used to decide whether to start/stop forwarding multicast
  // traffic to ARC.
  void NotifyAndroidIdleState(ArcBridgeService* bridge,
                              ::arc::mojom::IdleState enabled);

 private:
  class WakeLockRequestor;

  // Returns the WakeLockRequestor for |type|, creating one if needed.
  WakeLockRequestor* GetWakeLockRequestor(device::mojom::WakeLockType type);

  // Called on PowerManagerClient::GetScreenBrightnessPercent() completion.
  void OnGetScreenBrightnessPercent(std::optional<double> percent);

  // Called by Android when ready to suspend.
  void OnAndroidSuspendReady(base::UnguessableToken token);

  // Called by ConciergeClient when a response has been receive for the
  // SuspendVm D-Bus call.
  void OnConciergeSuspendVmResponse(
      base::UnguessableToken token,
      std::optional<vm_tools::concierge::SuspendVmResponse> reply);

  // Called by ConciergeClient when a response has been receive for the
  // ResumeVm D-Bus call.
  void OnConciergeResumeVmResponse(
      std::optional<vm_tools::concierge::ResumeVmResponse> reply);

  // Called on PowerManagerClient::GetBatterySaverModeState() completion.
  void OnBatterySaverModeStateReceived(
      GetBatterySaverModeStateCallback callback,
      std::optional<power_manager::BatterySaverModeState> state);

  // Sends a PowerInstance::UpdateScreenBrightnessSettings mojo call to Android.
  void UpdateAndroidScreenBrightness(double percent);

  // Sends a PowerInstance::Resume mojo call to Android.
  void DispatchAndroidResume();

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  std::string user_id_hash_;

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;

  // Used to track Android wake lock requests and acquire and release device
  // service wake locks as needed.
  std::map<device::mojom::WakeLockType, std::unique_ptr<WakeLockRequestor>>
      wake_lock_requestors_;

  // Last time that the power manager notified about a brightness change.
  base::TimeTicks last_brightness_changed_time_;
  // Timer used to run UpdateAndroidScreenBrightness() to notify Android
  // about brightness changes.
  base::OneShotTimer notify_brightness_timer_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;

  // Represents whether a device suspend is currently underway, ie. a
  // SuspendImminent event has been observed, but a SuspendDone event has not
  // yet been observed.
  bool is_suspending_ = false;

  // Controls whether or not we switch Android's idle state upon events.
  bool android_idle_control_disabled_ = false;

  base::WeakPtrFactory<ArcPowerBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_POWER_ARC_POWER_BRIDGE_H_
