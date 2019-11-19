// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/boot_phase_monitor.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Receives events regarding ARC boot phase from both ARC and Chrome, and do
// either one-time or continuous container priority adjustment / UMA recording
// in response.
class ArcBootPhaseMonitorBridge : public KeyedService,
                                  public mojom::BootPhaseMonitorHost,
                                  public ArcSessionManager::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void RecordFirstAppLaunchDelayUMA(base::TimeDelta delta) = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnBootCompleted() = 0;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcBootPhaseMonitorBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcBootPhaseMonitorBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // Records Arc.FirstAppLaunchDelay.TimeDelta UMA in the following way:
  //
  // * If ARC has already fully started, record the UMA with 0.
  // * If ARC hasn't fully started yet, record the UMA in OnBootCompleted()
  //   later.
  // * If |first_app_launch_delay_recorded_| is true, do nothing.
  //
  // This function must be called every time when Chrome browser tries to launch
  // an ARC app.
  static void RecordFirstAppLaunchDelayUMA(content::BrowserContext* context);

  ArcBootPhaseMonitorBridge(content::BrowserContext* context,
                            ArcBridgeService* bridge_service);
  ~ArcBootPhaseMonitorBridge() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // mojom::BootPhaseMonitorHost
  void OnBootCompleted() override;

  // ArcSessionManager::Observer
  void OnArcSessionStopped(ArcStopReason stop_reason) override;
  void OnArcSessionRestarting() override;

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);
  void RecordFirstAppLaunchDelayUMAForTesting() {
    RecordFirstAppLaunchDelayUMAInternal();
  }

 private:
  void RecordFirstAppLaunchDelayUMAInternal();
  void Reset();

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  const AccountId account_id_;
  std::unique_ptr<Delegate> delegate_;
  base::ObserverList<Observer> observers_;

  // The following variables must be reset every time when the instance stops or
  // restarts.
  base::TimeTicks app_launch_time_;
  bool first_app_launch_delay_recorded_ = false;
  bool boot_completed_ = false;

  // This has to be the last member variable in the class.
  base::WeakPtrFactory<ArcBootPhaseMonitorBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcBootPhaseMonitorBridge);
};

// Singleton factory for ArcBootPhaseMonitorBridge.
class ArcBootPhaseMonitorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcBootPhaseMonitorBridge,
          ArcBootPhaseMonitorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcBootPhaseMonitorBridgeFactory";

  static ArcBootPhaseMonitorBridgeFactory* GetInstance();

 private:
  friend base::DefaultSingletonTraits<ArcBootPhaseMonitorBridgeFactory>;

  ArcBootPhaseMonitorBridgeFactory() = default;

  ~ArcBootPhaseMonitorBridgeFactory() override = default;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
