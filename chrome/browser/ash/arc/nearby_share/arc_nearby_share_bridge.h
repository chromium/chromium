// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_ARC_NEARBY_SHARE_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_ARC_NEARBY_SHARE_BRIDGE_H_

#include <stdint.h>
#include <map>
#include <memory>

#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/nearby_share/nearby_share_session_impl.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Nearby Share related IPC from ARC++ and allows the Chrome
// Nearby Share UI to be displayed and managed in Chrome instead of the
// Android Nearby Share activity.
class ArcNearbyShareBridge : public KeyedService,
                             public mojom::NearbyShareHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the |browser_context| is not allowed to use ARC.
  static ArcNearbyShareBridge* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static ArcNearbyShareBridge* GetForBrowserContextForTesting(
      content::BrowserContext* browser_context);

  ArcNearbyShareBridge(content::BrowserContext* browser_context,
                       ArcBridgeService* bridge_service);

  ArcNearbyShareBridge(const ArcNearbyShareBridge&) = delete;
  ArcNearbyShareBridge& operator=(const ArcNearbyShareBridge&) = delete;
  ~ArcNearbyShareBridge() override;

  // mojom::NearbyShareHost overrides.
  void StartNearbyShare(
      uint32_t task_id,
      mojom::ShareIntentInfoPtr info,
      mojo::PendingRemote<mojom::NearbyShareSessionInstance> instance,
      StartNearbyShareCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  // Called by NearbyShareSessionImpl when the session is finished and can be
  // cleaned up.
  void OnNearbyShareSessionFinished(uint32_t task_id);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // Unowned pointer.
  const raw_ptr<Profile> profile_;

  // Map that keeps track of a task_id with its NearbyShareSessionImpl instance.
  std::map<uint32_t, std::unique_ptr<NearbyShareSessionImpl>> session_map_;

  base::WeakPtrFactory<ArcNearbyShareBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_ARC_NEARBY_SHARE_BRIDGE_H_
