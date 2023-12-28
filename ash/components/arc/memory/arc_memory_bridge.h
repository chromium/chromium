// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_MEMORY_ARC_MEMORY_BRIDGE_H_
#define ASH_COMPONENTS_ARC_MEMORY_ARC_MEMORY_BRIDGE_H_

#include "ash/components/arc/mojom/memory.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {
class ArcBridgeService;

// Collects information from other ArcServices and send UMA metrics.
class ArcMemoryBridge : public KeyedService {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMemoryBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMemoryBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcMemoryBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service);
  ArcMemoryBridge(const ArcMemoryBridge&) = delete;
  ArcMemoryBridge& operator=(const ArcMemoryBridge&) = delete;
  ~ArcMemoryBridge() override;

  // Drops the guest kernel's page caches.
  using DropCachesCallback = base::OnceCallback<void(bool)>;
  void DropCaches(DropCachesCallback callback);

  // Reclaims pages from all guest processes.
  using ReclaimCallback = base::OnceCallback<void(mojom::ReclaimResultPtr)>;
  void Reclaim(mojom::ReclaimRequestPtr, ReclaimCallback);

  static void EnsureFactoryBuilt();

 private:
  THREAD_CHECKER(thread_checker_);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_MEMORY_ARC_MEMORY_BRIDGE_H_
