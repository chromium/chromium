// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_IDLE_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_IDLE_MANAGER_H_

#include <memory>
#include <string>

#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/throttle/throttle_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/display/manager/display_configurator.h"

namespace arc {
class ArcBridgeService;

// This class holds a number of observers which watch for all conditions that
// gate the triggering of ARC's Idle (Doze) mode.
class ArcIdleManager : public KeyedService,
                       public ArcPowerBridge::Observer,
                       public ash::ThrottleService,
                       public display::DisplayConfigurator::Observer,
                       public ConnectionObserver<mojom::PowerInstance> {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Switches Android's so-called "Interactive Mode" ON (for |enable| true)
    // or OFF. The OFF setting is equivalent to the power-off button on a
    // smartphone (screen updates are turned off, leading to a progressive
    // power down of the system, including doze mode).
    // Switches are made via the Android |bridge|.
    virtual void SetIdleState(ArcPowerBridge* arc_power_bridge,
                              ArcBridgeService* bridge,
                              bool enable) = 0;
  };

  ArcIdleManager(content::BrowserContext* context, ArcBridgeService* bridge);

  ArcIdleManager(const ArcIdleManager&) = delete;
  ArcIdleManager& operator=(const ArcIdleManager&) = delete;

  ~ArcIdleManager() override;

  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use ARC.
  static ArcIdleManager* GetForBrowserContext(content::BrowserContext* context);
  static ArcIdleManager* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  static void EnsureFactoryBuilt();

  // KeyedService:
  void Shutdown() override;

  // ConnectionObserver<mojom::PowerInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // ArcPowerBridge::Observer
  void OnVmResumed() override;
  void OnWillDestroyArcPowerBridge() override;

  // Replaces the delegate so we can monitor switches without touching actual
  // power state, for unit test purposes.
  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  // ash::ThrottleService:
  // This is the main idle toggle.
  void ThrottleInstance(bool should_idle) override;

  // DisplayConfigurator::Observer:
  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override;

 private:
  void LogScreenOffTimer(bool toggle_timer);
  void RequestDozeWithoutMetrics(bool enabled);
  void RequestDoze(bool enabled);

  bool first_idle_happened_ = false;
  base::TimeDelta enable_delay_;
  std::unique_ptr<Delegate> delegate_;
  bool is_connected_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  SEQUENCE_CHECKER(sequence_checker_);

  // Owned by ArcServiceManager.
  const raw_ptr<ArcBridgeService> bridge_;
  raw_ptr<ArcPowerBridge> arc_power_bridge_;

  base::ElapsedTimer interactive_off_span_timer_;
  base::OneShotTimer enable_timer_;

  base::ScopedObservation<ArcPowerBridge, ArcPowerBridge::Observer>
      powerbridge_observation_{this};

  // During review, the team considered whether this notification
  // should come from ArcDisplayPowerObserver to preserve the wall
  // of abstraction between ArcIdleManager and its observers.
  // We decided this direct approach was the better way to go, as
  // the display state change triggers immediate configuration requests
  // in ArcIdleManager, and we already have a precedent for this
  // in OnVmMresumed().
  // In the future, if this pattern repeats frequently, may consider
  // refactoring.
  base::ScopedObservation<display::DisplayConfigurator,
                          display::DisplayConfigurator::Observer>
      display_observation_{this};

  base::WeakPtrFactory<ArcIdleManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_IDLE_MANAGER_H_
