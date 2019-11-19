// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_prefs.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/display/display_prefs.h"
#include "ash/login/login_screen_controller.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/media/media_controller_impl.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/network/vpn_list_view.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/power/power_prefs.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"

namespace ash {

namespace {

// Registers prefs whose default values are same in user and signin prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  AccessibilityControllerImpl::RegisterProfilePrefs(registry);
  AppListControllerImpl::RegisterProfilePrefs(registry);
  AssistantController::RegisterProfilePrefs(registry);
  BluetoothPowerController::RegisterProfilePrefs(registry);
  CapsLockNotificationController::RegisterProfilePrefs(registry, for_test);
  DockedMagnifierControllerImpl::RegisterProfilePrefs(registry);
  LoginScreenController::RegisterProfilePrefs(registry, for_test);
  LogoutButtonTray::RegisterProfilePrefs(registry);
  MediaControllerImpl::RegisterProfilePrefs(registry);
  MessageCenterController::RegisterProfilePrefs(registry);
  NightLightControllerImpl::RegisterProfilePrefs(registry);
  PaletteTray::RegisterProfilePrefs(registry);
  PaletteWelcomeBubble::RegisterProfilePrefs(registry);
  ShelfController::RegisterProfilePrefs(registry);
  TouchDevicesController::RegisterProfilePrefs(registry);
  tray::VPNListView::RegisterProfilePrefs(registry);

  // ash_unittests relies on assistant prefs.
  if (for_test)
    chromeos::assistant::prefs::RegisterProfilePrefs(registry);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry, bool for_test) {
  PaletteTray::RegisterLocalStatePrefs(registry);
  WallpaperControllerImpl::RegisterLocalStatePrefs(registry);
  BluetoothPowerController::RegisterLocalStatePrefs(registry);
  DetachableBaseHandler::RegisterPrefs(registry);
  PowerPrefs::RegisterLocalStatePrefs(registry);
  DisplayPrefs::RegisterLocalStatePrefs(registry);
}

void RegisterSigninProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  RegisterProfilePrefs(registry, for_test);
  PowerPrefs::RegisterSigninProfilePrefs(registry);
}

void RegisterUserProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  RegisterProfilePrefs(registry, for_test);
  PowerPrefs::RegisterUserProfilePrefs(registry);
}

}  // namespace ash
