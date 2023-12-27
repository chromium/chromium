// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_
#define ASH_COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_

#include <map>
#include <memory>

#include "ash/components/arc/mojom/wake_lock.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Sets wake up timers / alarms based on calls from the instance.
class ArcWakeLockBridge : public KeyedService,
                          public ConnectionObserver<mojom::WakeLockInstance>,
                          public mojom::WakeLockHost {
 public:
  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcWakeLockBridge* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcWakeLockBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcWakeLockBridge(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);

  ArcWakeLockBridge(const ArcWakeLockBridge&) = delete;
  ArcWakeLockBridge& operator=(const ArcWakeLockBridge&) = delete;

  ~ArcWakeLockBridge() override;

  void SetWakeLockProviderForTesting(
      mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider) {
    wake_lock_provider_ = std::move(wake_lock_provider);
  }

  // ConnectionObserver<mojom::WakeLockInstance>::Observer overrides.
  void OnConnectionClosed() override;

  // Runs the message loop until replies have been received for all pending
  // device service requests in |wake_lock_requesters_|.
  void FlushWakeLocksForTesting();

  // mojom::WakeLockHost overrides.
  void AcquirePartialWakeLock(AcquirePartialWakeLockCallback callback) override;
  void ReleasePartialWakeLock(ReleasePartialWakeLockCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  class WakeLockRequester;

  // Returns the WakeLockRequester for |type|, creating one if needed.
  WakeLockRequester* GetWakeLockRequester(device::mojom::WakeLockType type);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;

  // Used to track Android wake lock requests and acquire and release device
  // service wake locks as needed.
  std::map<device::mojom::WakeLockType, std::unique_ptr<WakeLockRequester>>
      wake_lock_requesters_;

  mojo::Receiver<mojom::WakeLockHost> receiver_{this};

  base::WeakPtrFactory<ArcWakeLockBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_
