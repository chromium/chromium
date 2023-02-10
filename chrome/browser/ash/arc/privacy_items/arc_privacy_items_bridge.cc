// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/privacy_items/arc_privacy_items_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/singleton.h"

namespace arc {

namespace {

// Singleton factory for ArcPrivacyItemsBridge.
class ArcPrivacyItemsBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPrivacyItemsBridge,
          ArcPrivacyItemsBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPrivacyItemsBridgeFactory";

  static ArcPrivacyItemsBridgeFactory* GetInstance() {
    return base::Singleton<ArcPrivacyItemsBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPrivacyItemsBridgeFactory>;

  ArcPrivacyItemsBridgeFactory() = default;
  ~ArcPrivacyItemsBridgeFactory() override = default;
};

}  // namespace

// static
ArcPrivacyItemsBridge* ArcPrivacyItemsBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPrivacyItemsBridgeFactory::GetForBrowserContext(context);
}

ArcPrivacyItemsBridge::ArcPrivacyItemsBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  DVLOG(2) << "ArcPrivacyItemsBridge::ArcPrivacyItemsBridge";
  arc_bridge_service_->privacy_items()->SetHost(this);
  arc_bridge_service_->privacy_items()->AddObserver(this);
}

ArcPrivacyItemsBridge::~ArcPrivacyItemsBridge() {
  DVLOG(2) << "ArcPrivacyItemsBridge::~ArcPrivacyItemsBridge";
  arc_bridge_service_->privacy_items()->RemoveObserver(this);
  arc_bridge_service_->privacy_items()->SetHost(nullptr);
}

void ArcPrivacyItemsBridge::OnPrivacyItemsChanged(
    std::vector<arc::mojom::PrivacyItemPtr> privacy_items) {
  DVLOG(1) << "ArcPrivacyItemsBridge::OnPrivacyItemsChanged size="
           << privacy_items.size();

  for (auto& observer : observer_list_)
    observer.OnPrivacyItemsChanged(privacy_items);
}

void ArcPrivacyItemsBridge::OnMicCameraIndicatorRequirementChanged(bool flag) {
  DVLOG(1) << "ArcPrivacyItemsBridge::OnMicCameraIndicatorRequirementChanged "
              "required="
           << flag;
}

void ArcPrivacyItemsBridge::OnLocationIndicatorRequirementChanged(bool flag) {
  DVLOG(1) << "ArcPrivacyItemsBridge::OnLocationIndicatorRequirementChanged "
              "required="
           << flag;
}

void ArcPrivacyItemsBridge::OnStaticPrivacyIndicatorBoundsChanged(
    int32_t display_id,
    std::vector<gfx::Rect> bounds) {
  DVLOG(1) << "ArcPrivacyItemsBridge::OnStaticPrivacyIndicatorBoundsChanged";

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->privacy_items(),
                                  OnStaticPrivacyIndicatorBoundsChanged);
  if (!instance)
    return;

  instance->OnStaticPrivacyIndicatorBoundsChanged(display_id, bounds);
}

void ArcPrivacyItemsBridge::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcPrivacyItemsBridge::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// static
void ArcPrivacyItemsBridge::EnsureFactoryBuilt() {
  ArcPrivacyItemsBridgeFactory::GetInstance();
}

}  // namespace arc
