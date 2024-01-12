// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_PRIVACY_ITEMS_ARC_PRIVACY_ITEMS_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_PRIVACY_ITEMS_ARC_PRIVACY_ITEMS_BRIDGE_H_

#include "ash/components/arc/mojom/privacy_items.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {

class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcPrivacyItemsBridge
    : public KeyedService,
      public ConnectionObserver<mojom::PrivacyItemsInstance>,
      public mojom::PrivacyItemsHost {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when `privacy_items` were changed.
    virtual void OnPrivacyItemsChanged(
        const std::vector<arc::mojom::PrivacyItemPtr>& privacy_items) {}
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPrivacyItemsBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcPrivacyItemsBridge(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);

  ArcPrivacyItemsBridge(const ArcPrivacyItemsBridge&) = delete;
  ArcPrivacyItemsBridge& operator=(const ArcPrivacyItemsBridge&) = delete;

  ~ArcPrivacyItemsBridge() override;

  // PrivacyItemsHost overrides.
  void OnPrivacyItemsChanged(
      std::vector<arc::mojom::PrivacyItemPtr> privacy_items) override;
  void OnMicCameraIndicatorRequirementChanged(bool flag) override;
  void OnLocationIndicatorRequirementChanged(bool flag) override;

  // PrivacyItemsInstance methods:
  void OnStaticPrivacyIndicatorBoundsChanged(int32_t display_id,
                                             std::vector<gfx::Rect> bounds);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService> arc_bridge_service_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_PRIVACY_ITEMS_ARC_PRIVACY_ITEMS_BRIDGE_H_
