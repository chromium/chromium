// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_prefs.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/display/display_prefs.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/session/fullscreen_controller.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/gesture_education/gesture_education_notification_controller.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/network/cellular_setup_notifier.h"
#include "ash/system/network/vpn_list_view.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"
#include "ash/system/power/power_prefs.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/unified/hps_notify_controller.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/persistent_desks_bar_controller.h"
#include "ash/wm/desks/templates/desks_templates_util.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"

namespace ash {

namespace {

// Registers prefs whose default values are same in user and signin prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  AcceleratorControllerImpl::RegisterProfilePrefs(registry);
  AccessibilityControllerImpl::RegisterProfilePrefs(registry);
  AppListControllerImpl::RegisterProfilePrefs(registry);
  AssistantControllerImpl::RegisterProfilePrefs(registry);
  AshColorProvider::RegisterProfilePrefs(registry);
  AmbientController::RegisterProfilePrefs(registry);
  if (!ash::features::IsBluetoothRevampEnabled())
    BluetoothPowerController::RegisterProfilePrefs(registry);
  CapsLockNotificationController::RegisterProfilePrefs(registry, for_test);
  CaptureModeController::RegisterProfilePrefs(registry);
  CellularSetupNotifier::RegisterProfilePrefs(registry);
  contextual_tooltip::RegisterProfilePrefs(registry);
  ClipboardNudgeController::RegisterProfilePrefs(registry);
  desks_restore_util::RegisterProfilePrefs(registry);
  desks_templates_util::RegisterProfilePrefs(registry);
  DockedMagnifierController::RegisterProfilePrefs(registry);
  FullscreenController::RegisterProfilePrefs(registry);
  GestureEducationNotificationController::RegisterProfilePrefs(registry,
                                                               for_test);
  holding_space_prefs::RegisterProfilePrefs(registry);
  HpsNotifyController::RegisterProfilePrefs(registry);
  LoginScreenController::RegisterProfilePrefs(registry, for_test);
  LogoutButtonTray::RegisterProfilePrefs(registry);
  LogoutConfirmationController::RegisterProfilePrefs(registry);
  KeyboardControllerImpl::RegisterProfilePrefs(registry);
  MediaControllerImpl::RegisterProfilePrefs(registry);
  MessageCenterController::RegisterProfilePrefs(registry);
  NightLightControllerImpl::RegisterProfilePrefs(registry);
  PaletteTray::RegisterProfilePrefs(registry);
  PaletteWelcomeBubble::RegisterProfilePrefs(registry);
  PciePeripheralNotificationController::RegisterProfilePrefs(registry);
  PersistentDesksBarController::RegisterProfilePrefs(registry);
  PrivacyScreenController::RegisterProfilePrefs(registry);
  quick_pair::Mediator::RegisterProfilePrefs(registry);
  ShelfController::RegisterProfilePrefs(registry);
  TouchDevicesController::RegisterProfilePrefs(registry, for_test);
  tray::VPNListView::RegisterProfilePrefs(registry);
  UnifiedSystemTrayController::RegisterProfilePrefs(registry);
  MediaTray::RegisterProfilePrefs(registry);
  UsbPeripheralNotificationController::RegisterProfilePrefs(registry);
  WallpaperControllerImpl::RegisterProfilePrefs(registry);
  WindowCycleController::RegisterProfilePrefs(registry);

  // Provide prefs registered in the browser for ash_unittests.
  if (for_test) {
    chromeos::assistant::prefs::RegisterProfilePrefs(registry);
    quick_answers::prefs::RegisterProfilePrefs(registry);
    registry->RegisterBooleanPref(prefs::kMouseReverseScroll, false);
    registry->RegisterBooleanPref(prefs::kSendFunctionKeys, false);
    registry->RegisterBooleanPref(chromeos::prefs::kSuggestedContentEnabled,
                                  true);
    registry->RegisterBooleanPref(::prefs::kLiveCaptionEnabled, false);
    registry->RegisterStringPref(language::prefs::kApplicationLocale,
                                 std::string());
  }
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry, bool for_test) {
  PaletteTray::RegisterLocalStatePrefs(registry);
  WallpaperControllerImpl::RegisterLocalStatePrefs(registry);
  if (!ash::features::IsBluetoothRevampEnabled())
    BluetoothPowerController::RegisterLocalStatePrefs(registry);
  DetachableBaseHandler::RegisterPrefs(registry);
  PowerPrefs::RegisterLocalStatePrefs(registry);
  DisplayPrefs::RegisterLocalStatePrefs(registry);
  LoginExpandedPublicAccountView::RegisterLocalStatePrefs(registry);
  LockStateController::RegisterPrefs(registry);
  quick_pair::Mediator::RegisterLocalStatePrefs(registry);
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
