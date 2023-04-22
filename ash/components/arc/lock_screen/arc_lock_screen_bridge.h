// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_LOCK_SCREEN_ARC_LOCK_SCREEN_BRIDGE_H_
#define ASH_COMPONENTS_ARC_LOCK_SCREEN_ARC_LOCK_SCREEN_BRIDGE_H_

#include "ash/components/arc/mojom/lock_screen.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class notifies the Chrome OS side lock screen state to the container.
class ArcLockScreenBridge
    : public KeyedService,
      public ConnectionObserver<mojom::LockScreenInstance>,
      public session_manager::SessionManagerObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcLockScreenBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcLockScreenBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcLockScreenBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);

  ArcLockScreenBridge(const ArcLockScreenBridge&) = delete;
  ArcLockScreenBridge& operator=(const ArcLockScreenBridge&) = delete;

  ~ArcLockScreenBridge() override;

  // ConnectionObserver<mojom::LockScreenInstance> overrides:
  void OnConnectionReady() override;

  // session_manager::SessionManagerObserver overrides.
  void OnSessionStateChanged() override;

  static void EnsureFactoryBuilt();

 private:
  // Sends the device locked state to container.
  void SendDeviceLockedState();

  THREAD_CHECKER(thread_checker_);

  const raw_ptr<ArcBridgeService, ExperimentalAsh>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_LOCK_SCREEN_ARC_LOCK_SCREEN_BRIDGE_H_
