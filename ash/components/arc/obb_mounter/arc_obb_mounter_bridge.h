// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_
#define ASH_COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_

#include <string>

#include "ash/components/arc/mojom/obb_mounter.mojom.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles OBB mount/unmount requests from Android.
class ArcObbMounterBridge : public KeyedService, public mojom::ObbMounterHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcObbMounterBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcObbMounterBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);

  ArcObbMounterBridge(const ArcObbMounterBridge&) = delete;
  ArcObbMounterBridge& operator=(const ArcObbMounterBridge&) = delete;

  ~ArcObbMounterBridge() override;

  // mojom::ObbMounterHost overrides:
  void MountObb(const std::string& obb_file,
                const std::string& target_path,
                int32_t owner_gid,
                MountObbCallback callback) override;
  void UnmountObb(const std::string& target_path,
                  UnmountObbCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_
