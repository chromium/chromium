// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/rotation_lock/arc_rotation_lock_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/singleton.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"

namespace arc {

namespace {

// Singleton factory for ArcRotationLockBridge.
class ArcRotationLockBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcRotationLockBridge,
          ArcRotationLockBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcRotationLockBridgeFactory";

  static ArcRotationLockBridgeFactory* GetInstance() {
    return base::Singleton<ArcRotationLockBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcRotationLockBridgeFactory>;
  ArcRotationLockBridgeFactory() = default;
  ~ArcRotationLockBridgeFactory() override = default;
};

}  // namespace

// static
ArcRotationLockBridge* ArcRotationLockBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcRotationLockBridgeFactory::GetForBrowserContext(context);
}

// static
ArcRotationLockBridge* ArcRotationLockBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcRotationLockBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcRotationLockBridge::ArcRotationLockBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->rotation_lock()->AddObserver(this);
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->screen_orientation_controller()->AddObserver(this);
    ash::Shell::Get()->tablet_mode_controller()->AddObserver(this);
  }
}

ArcRotationLockBridge::~ArcRotationLockBridge() {
  arc_bridge_service_->rotation_lock()->RemoveObserver(this);
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->screen_orientation_controller()->RemoveObserver(this);
    ash::Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  }
}

void ArcRotationLockBridge::OnConnectionReady() {
  SendRotationLockState();
}

void ArcRotationLockBridge::OnUserRotationLockChanged() {
  SendRotationLockState();
}

void ArcRotationLockBridge::OnTabletPhysicalStateChanged() {
  SendRotationLockState();
}

void ArcRotationLockBridge::SendRotationLockState() {
  // ash::Shell may not exist in tests.
  if (!ash::Shell::HasInstance())
    return;

  mojom::RotationLockInstance* rotation_lock_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->rotation_lock(),
                                  OnRotationLockStateChanged);
  if (!rotation_lock_instance)
    return;

  display::Display current_display;
  if (display::HasInternalDisplay()) {
    bool found = display::Screen::GetScreen()->GetDisplayWithDisplayId(
        display::Display::InternalDisplayId(), &current_display);
    DCHECK(found);
  }

  auto* screen_orientation_controller =
      ash::Shell::Get()->screen_orientation_controller();
  const bool accelerometer_active =
      screen_orientation_controller->IsAutoRotationAllowed() &&
      !screen_orientation_controller->rotation_locked();

  rotation_lock_instance->OnRotationLockStateChanged(
      accelerometer_active,
      static_cast<arc::mojom::Rotation>(current_display.rotation()));
}

// static
void ArcRotationLockBridge::EnsureFactoryBuilt() {
  ArcRotationLockBridgeFactory::GetInstance();
}

}  // namespace arc
