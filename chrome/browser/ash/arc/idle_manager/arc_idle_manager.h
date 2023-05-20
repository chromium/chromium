// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_IDLE_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_IDLE_MANAGER_H_

#include <memory>
#include <string>

#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/throttle_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace arc {
class ArcBridgeService;

// This class holds a number of observers which watch for all conditions that
// gate the triggering of ARC's Idle (Doze) mode.
class ArcIdleManager : public KeyedService,
                       public ash::ThrottleService,
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
    virtual void SetInteractiveMode(ArcBridgeService* bridge, bool enable) = 0;
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

  // Replaces the delegate so we can monitor switches without touching actual
  // power state, for unit test purposes.
  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  // ash::ThrottleService:
  // This is the main idle toggle.
  void ThrottleInstance(bool should_idle) override;

 private:
  std::unique_ptr<Delegate> delegate_;
  bool is_connected_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  SEQUENCE_CHECKER(sequence_checker_);

  void LogScreenOffTimer(bool should_throttle);

  // Owned by ArcServiceManager.
  const raw_ptr<ArcBridgeService, ExperimentalAsh> bridge_;

  base::ElapsedTimer interactive_off_span_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_IDLE_MANAGER_H_
