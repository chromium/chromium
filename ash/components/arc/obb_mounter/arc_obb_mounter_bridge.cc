// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/obb_mounter/arc_obb_mounter_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/dbus/arc/arc_obb_mounter_client.h"

namespace arc {

namespace {

// Singleton factory for ArcObbMounterBridge.
class ArcObbMounterBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcObbMounterBridge,
          ArcObbMounterBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcObbMounterBridgeFactory";

  static ArcObbMounterBridgeFactory* GetInstance() {
    return base::Singleton<ArcObbMounterBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcObbMounterBridgeFactory>;
  ArcObbMounterBridgeFactory() = default;
  ~ArcObbMounterBridgeFactory() override = default;
};

}  // namespace

// static
ArcObbMounterBridge* ArcObbMounterBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcObbMounterBridgeFactory::GetForBrowserContext(context);
}

ArcObbMounterBridge::ArcObbMounterBridge(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->obb_mounter()->SetHost(this);
}

ArcObbMounterBridge::~ArcObbMounterBridge() {
  arc_bridge_service_->obb_mounter()->SetHost(nullptr);
}

void ArcObbMounterBridge::MountObb(const std::string& obb_file,
                                   const std::string& target_path,
                                   int32_t owner_gid,
                                   MountObbCallback callback) {
  ash::ArcObbMounterClient::Get()->MountObb(obb_file, target_path, owner_gid,
                                            std::move(callback));
}

void ArcObbMounterBridge::UnmountObb(const std::string& target_path,
                                     UnmountObbCallback callback) {
  ash::ArcObbMounterClient::Get()->UnmountObb(target_path, std::move(callback));
}

// static
void ArcObbMounterBridge::EnsureFactoryBuilt() {
  ArcObbMounterBridgeFactory::GetInstance();
}

}  // namespace arc
