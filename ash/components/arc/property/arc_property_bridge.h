// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_PROPERTY_ARC_PROPERTY_BRIDGE_H_
#define ASH_COMPONENTS_ARC_PROPERTY_ARC_PROPERTY_BRIDGE_H_

#include <vector>

#include "ash/components/arc/mojom/property.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// ARC Property Client gets system properties from ARC instances.
// TODO(yhanada): Remove this class entirely once the other end of IPC is
// cleaned up.
class ArcPropertyBridge : public KeyedService,
                          public ConnectionObserver<mojom::PropertyInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPropertyBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcPropertyBridge(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcPropertyBridge() override;
  ArcPropertyBridge(const ArcPropertyBridge&) = delete;
  ArcPropertyBridge& operator=(const ArcPropertyBridge&) = delete;

  // ConnectionObserver<mojom::PropertyInstance> overrides:
  void OnConnectionReady() override;

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService, ExperimentalAsh>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // Store pending requests when connection is not ready.
  std::vector<mojom::PropertyInstance::GetGcaMigrationPropertyCallback>
      pending_requests_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_PROPERTY_ARC_PROPERTY_BRIDGE_H_
