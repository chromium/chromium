// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/system_ui/arc_system_ui_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/constants/ash_features.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcSystemUIBridge.
class ArcSystemUIBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSystemUIBridge,
          ArcSystemUIBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSystemUIBridgeFactory";

  static ArcSystemUIBridgeFactory* GetInstance() {
    return base::Singleton<ArcSystemUIBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcSystemUIBridgeFactory>;
  ArcSystemUIBridgeFactory() = default;
  ~ArcSystemUIBridgeFactory() override = default;
};

}  // namespace

// static
ArcSystemUIBridge* ArcSystemUIBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSystemUIBridgeFactory::GetForBrowserContext(context);
}

// static
ArcSystemUIBridge* ArcSystemUIBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcSystemUIBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcSystemUIBridge::ArcSystemUIBridge(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  // `dark_light_mode_controller` might be nullptr in unit tests.
  if (auto* dark_light_mode_controller =
          ash::DarkLightModeControllerImpl::Get()) {
    dark_light_mode_controller->AddObserver(this);
  }
  arc_bridge_service_->system_ui()->AddObserver(this);
}

ArcSystemUIBridge::~ArcSystemUIBridge() {
  // `dark_light_mode_controller` might be nullptr in unit tests.
  if (auto* dark_light_mode_controller =
          ash::DarkLightModeControllerImpl::Get()) {
    dark_light_mode_controller->RemoveObserver(this);
  }
  arc_bridge_service_->system_ui()->RemoveObserver(this);
}

void ArcSystemUIBridge::OnConnectionReady() {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  bool dark_theme_status = false;
  // Checking to see if the flag is enabled because provider returns dark
  // mode when the flag is default.
  if (dark_light_mode_controller && ash::features::IsDarkLightModeEnabled())
    dark_theme_status = dark_light_mode_controller->IsDarkModeEnabled();

  if (!ArcSystemUIBridge::SendDeviceDarkThemeState(dark_theme_status)) {
    LOG(ERROR) << "OnConnectionReady failed to get System UI instance for "
                  "initial dark theme status : "
               << dark_theme_status;
  }
}

void ArcSystemUIBridge::OnColorModeChanged(bool dark_theme_status) {
  if (!ArcSystemUIBridge::SendDeviceDarkThemeState(dark_theme_status)) {
    LOG(ERROR) << "OnColorModeChanged failed to get System UI instance for "
                  "the change in dark theme status to : "
               << dark_theme_status;
  }
}

bool ArcSystemUIBridge::SendDeviceDarkThemeState(bool dark_theme_status) {
  mojom::SystemUiInstance* system_ui_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->system_ui(), SetDarkThemeStatus);

  if (!system_ui_instance)
    return false;
  system_ui_instance->SetDarkThemeStatus(dark_theme_status);
  return true;
}

bool ArcSystemUIBridge::SendOverlayColor(uint32_t source_color,
                                         mojom::ThemeStyleType theme_style) {
  mojom::SystemUiInstance* system_ui_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->system_ui(), SetOverlayColor);

  if (!system_ui_instance)
    return false;
  system_ui_instance->SetOverlayColor(source_color, theme_style);
  return true;
}

// static
void ArcSystemUIBridge::EnsureFactoryBuilt() {
  ArcSystemUIBridgeFactory::GetInstance();
}

}  // namespace arc
