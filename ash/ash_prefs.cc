// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_prefs.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/display/display_prefs.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/gesture_education/gesture_education_notification_controller.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/network/vpn_list_view.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/power/power_prefs.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {

namespace {

// Registers prefs whose default values are same in user and signin prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  AccessibilityControllerImpl::RegisterProfilePrefs(registry);
  AppListControllerImpl::RegisterProfilePrefs(registry);
  AssistantControllerImpl::RegisterProfilePrefs(registry);
  AshColorProvider::RegisterProfilePrefs(registry);
  AmbientController::RegisterProfilePrefs(registry);
  BluetoothPowerController::RegisterProfilePrefs(registry);
  CapsLockNotificationController::RegisterProfilePrefs(registry, for_test);
  contextual_tooltip::RegisterProfilePrefs(registry);
  ClipboardNudgeController::RegisterProfilePrefs(registry);
  desks_restore_util::RegisterProfilePrefs(registry);
  DockedMagnifierControllerImpl::RegisterProfilePrefs(registry);
  GestureEducationNotificationController::RegisterProfilePrefs(registry,
                                                               for_test);
  LoginScreenController::RegisterProfilePrefs(registry, for_test);
  LogoutButtonTray::RegisterProfilePrefs(registry);
  KeyboardControllerImpl::RegisterProfilePrefs(registry);
  MediaControllerImpl::RegisterProfilePrefs(registry);
  MessageCenterController::RegisterProfilePrefs(registry);
  NightLightControllerImpl::RegisterProfilePrefs(registry);
  PaletteTray::RegisterProfilePrefs(registry);
  PaletteWelcomeBubble::RegisterProfilePrefs(registry);
  PrivacyScreenController::RegisterProfilePrefs(registry);
  ShelfController::RegisterProfilePrefs(registry);
  TouchDevicesController::RegisterProfilePrefs(registry, for_test);
  tray::VPNListView::RegisterProfilePrefs(registry);
  WmGestureHandler::RegisterProfilePrefs(registry);
  MediaTray::RegisterProfilePrefs(registry);

  // Provide prefs registered in the browser for ash_unittests.
  if (for_test) {
    chromeos::assistant::prefs::RegisterProfilePrefs(registry);
    chromeos::quick_answers::prefs::RegisterProfilePrefs(registry);
    registry->RegisterBooleanPref(
        prefs::kMouseReverseScroll, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
    registry->RegisterBooleanPref(
        chromeos::prefs::kSuggestedContentEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  }
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry, bool for_test) {
  PaletteTray::RegisterLocalStatePrefs(registry);
  WallpaperControllerImpl::RegisterLocalStatePrefs(registry);
  BluetoothPowerController::RegisterLocalStatePrefs(registry);
  DetachableBaseHandler::RegisterPrefs(registry);
  PowerPrefs::RegisterLocalStatePrefs(registry);
  DisplayPrefs::RegisterLocalStatePrefs(registry);
  LoginExpandedPublicAccountView::RegisterLocalStatePrefs(registry);
  TopShortcutsView::RegisterLocalStatePrefs(registry);
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
