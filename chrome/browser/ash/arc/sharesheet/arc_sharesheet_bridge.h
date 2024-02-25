// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SHARESHEET_ARC_SHARESHEET_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_SHARESHEET_ARC_SHARESHEET_BRIDGE_H_

#include "ash/components/arc/mojom/sharesheet.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles sharesheet related IPC from ARC++ and allows sharesheet
// to be displayed and managed in Chrome preview instead of the Android
// sharesheet activity.
class ArcSharesheetBridge : public KeyedService, public mojom::SharesheetHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSharesheetBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcSharesheetBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcSharesheetBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ArcSharesheetBridge(const ArcSharesheetBridge&) = delete;
  ArcSharesheetBridge& operator=(const ArcSharesheetBridge&) = delete;
  ~ArcSharesheetBridge() override;

  // mojom::SharesheetHost overrides:
  // TODO(phshah): Add overrides.

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ArcSharesheetBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SHARESHEET_ARC_SHARESHEET_BRIDGE_H_
