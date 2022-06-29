// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/dark_theme/arc_dark_theme_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcDarkThemeBridge.
class ArcDarkThemeBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcDarkThemeBridge,
          ArcDarkThemeBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcDarkThemeBridgeFactory";

  static ArcDarkThemeBridgeFactory* GetInstance() {
    return base::Singleton<ArcDarkThemeBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcDarkThemeBridgeFactory>;
  ArcDarkThemeBridgeFactory() = default;
  ~ArcDarkThemeBridgeFactory() override = default;
};

}  // namespace

// static
ArcDarkThemeBridge* ArcDarkThemeBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcDarkThemeBridgeFactory::GetForBrowserContext(context);
}

// static
ArcDarkThemeBridge* ArcDarkThemeBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcDarkThemeBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcDarkThemeBridge::ArcDarkThemeBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  // `dark_light_mode_controller` might be nullptr in unit tests.
  if (auto* dark_light_mode_controller =
          ash::DarkLightModeControllerImpl::Get()) {
    dark_light_mode_controller->AddObserver(this);
  }
  arc_bridge_service_->dark_theme()->AddObserver(this);
}

ArcDarkThemeBridge::~ArcDarkThemeBridge() {
  // `dark_light_mode_controller` might be nullptr in unit tests.
  if (auto* dark_light_mode_controller =
          ash::DarkLightModeControllerImpl::Get()) {
    dark_light_mode_controller->RemoveObserver(this);
  }
  arc_bridge_service_->dark_theme()->RemoveObserver(this);
}

void ArcDarkThemeBridge::OnConnectionReady() {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  bool dark_theme_status = false;
  // Checking to see if the flag is enabled because provider returns dark mode
  // when the flag is default.
  if (dark_light_mode_controller && ash::features::IsDarkLightModeEnabled())
    dark_theme_status = dark_light_mode_controller->IsDarkModeEnabled();

  if (!ArcDarkThemeBridge::SendDeviceDarkThemeState(dark_theme_status)) {
    LOG(ERROR) << "OnConnectionReady failed to get Dark Theme instance for "
                  "initial dark theme status : "
               << dark_theme_status;
  }
}

void ArcDarkThemeBridge::OnColorModeChanged(bool dark_theme_status) {
  if (!ArcDarkThemeBridge::SendDeviceDarkThemeState(dark_theme_status)) {
    LOG(ERROR) << "OnColorModeChanged failed to get Dark Theme instance for "
                  "the change in dark theme status to : "
               << dark_theme_status;
  }
}

bool ArcDarkThemeBridge::SendDeviceDarkThemeState(bool dark_theme_status) {
  mojom::DarkThemeInstance* dark_theme_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->dark_theme(), DarkThemeStatus);
  if (!dark_theme_instance)
    return false;
  dark_theme_instance->DarkThemeStatus(dark_theme_status);
  return true;
}

bool ArcDarkThemeBridge::SendDeviceDarkThemeStateForTesting(
    bool dark_theme_status) {
  return ArcDarkThemeBridge::SendDeviceDarkThemeState(dark_theme_status);
}

}  // namespace arc
