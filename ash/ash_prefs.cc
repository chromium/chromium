// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_prefs.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/display/display_prefs.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/glanceables/glanceables_util.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/media/media_controller_impl.h"
#include "ash/metrics/feature_discovery_duration_reporter_impl.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/session/fullscreen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/camera/autozoom_nudge_controller.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/gesture_education/gesture_education_notification_controller.h"
#include "ash/system/human_presence/snooping_protection_controller.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/keyboard_modifier_metrics_recorder.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/network/cellular_setup_notifier.h"
#include "ash/system/network/vpn_list_view.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"
#include "ash/system/power/power_prefs.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_controller.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"

namespace ash {

namespace {

// Registers prefs whose default values are same in user and signin prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  AccessibilityControllerImpl::RegisterProfilePrefs(registry);
  AppListControllerImpl::RegisterProfilePrefs(registry);
  AssistantControllerImpl::RegisterProfilePrefs(registry);
  AutozoomControllerImpl::RegisterProfilePrefs(registry);
  AutozoomNudgeController::RegisterProfilePrefs(registry);
  AmbientController::RegisterProfilePrefs(registry);
  CalendarController::RegisterProfilePrefs(registry);
  CameraEffectsController::RegisterProfilePrefs(registry);
  CaptureModeController::RegisterProfilePrefs(registry);
  CellularSetupNotifier::RegisterProfilePrefs(registry);
  contextual_tooltip::RegisterProfilePrefs(registry);
  ClipboardNudgeController::RegisterProfilePrefs(registry);
  ColorPaletteController::RegisterPrefs(registry);
  DarkLightModeControllerImpl::RegisterProfilePrefs(registry);
  desks_restore_util::RegisterProfilePrefs(registry);
  saved_desk_util::RegisterProfilePrefs(registry);
  DockedMagnifierController::RegisterProfilePrefs(registry);
  FeatureDiscoveryDurationReporterImpl::RegisterProfilePrefs(registry);
  FullscreenController::RegisterProfilePrefs(registry);
  GestureEducationNotificationController::RegisterProfilePrefs(registry,
                                                               for_test);
  holding_space_prefs::RegisterProfilePrefs(registry);
  InputDeviceSettingsControllerImpl::RegisterProfilePrefs(registry);
  InputDeviceTracker::RegisterProfilePrefs(registry);
  LoginScreenController::RegisterProfilePrefs(registry, for_test);
  LogoutButtonTray::RegisterProfilePrefs(registry);
  LogoutConfirmationController::RegisterProfilePrefs(registry);
  KeyboardBacklightColorController::RegisterPrefs(registry);
  KeyboardControllerImpl::RegisterProfilePrefs(registry);
  KeyboardModifierMetricsRecorder::RegisterProfilePrefs(registry, for_test);
  MediaControllerImpl::RegisterProfilePrefs(registry);
  MessageCenterController::RegisterProfilePrefs(registry);
  NightLightControllerImpl::RegisterProfilePrefs(registry);
  PaletteTray::RegisterProfilePrefs(registry);
  PaletteWelcomeBubble::RegisterProfilePrefs(registry);
  PciePeripheralNotificationController::RegisterProfilePrefs(registry);
  PersistentDesksBarController::RegisterProfilePrefs(registry);
  PrivacyHubController::RegisterProfilePrefs(registry);
  PrivacyScreenController::RegisterProfilePrefs(registry);
  ProjectorControllerImpl::RegisterProfilePrefs(registry);
  quick_pair::Mediator::RegisterProfilePrefs(registry);
  ShelfController::RegisterProfilePrefs(registry);
  SnoopingProtectionController::RegisterProfilePrefs(registry);
  TouchDevicesController::RegisterProfilePrefs(registry, for_test);
  UnifiedSystemTrayController::RegisterProfilePrefs(registry);
  MediaTray::RegisterProfilePrefs(registry);
  UsbPeripheralNotificationController::RegisterProfilePrefs(registry);
  VPNListView::RegisterProfilePrefs(registry);
  WallpaperPrefManager::RegisterProfilePrefs(registry);
  WindowCycleController::RegisterProfilePrefs(registry);
  chromeos::MultitaskMenuNudgeController::RegisterProfilePrefs(registry);

  // Provide prefs registered in the browser for ash_unittests.
  if (for_test) {
    assistant::prefs::RegisterProfilePrefs(registry);
    quick_answers::prefs::RegisterProfilePrefs(registry);
    registry->RegisterBooleanPref(prefs::kMouseReverseScroll, false);
    registry->RegisterBooleanPref(prefs::kSendFunctionKeys, false);
    registry->RegisterBooleanPref(prefs::kSuggestedContentEnabled, true);
    registry->RegisterBooleanPref(::prefs::kLiveCaptionEnabled, false);
    registry->RegisterListPref(
        chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList);
    registry->RegisterStringPref(::prefs::kLiveCaptionLanguageCode,
                                 speech::kUsEnglishLocale);
    registry->RegisterStringPref(language::prefs::kApplicationLocale,
                                 std::string());
    registry->RegisterStringPref(language::prefs::kPreferredLanguages,
                                 std::string());
  }
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry, bool for_test) {
  PaletteTray::RegisterLocalStatePrefs(registry);
  WallpaperPrefManager::RegisterLocalStatePrefs(registry);
  DetachableBaseHandler::RegisterPrefs(registry);
  PowerPrefs::RegisterLocalStatePrefs(registry);
  DisplayPrefs::RegisterLocalStatePrefs(registry);
  LoginExpandedPublicAccountView::RegisterLocalStatePrefs(registry);
  LockStateController::RegisterPrefs(registry);
  quick_pair::Mediator::RegisterLocalStatePrefs(registry);
  if (ash::features::IsQsRevampEnabled())
    QuickSettingsFooter::RegisterLocalStatePrefs(registry);
  else
    TopShortcutsView::RegisterLocalStatePrefs(registry);
  KeyboardBacklightColorController::RegisterPrefs(registry);
}

void RegisterSigninProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  RegisterProfilePrefs(registry, for_test);
  PowerPrefs::RegisterSigninProfilePrefs(registry);
}

void RegisterUserProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  RegisterProfilePrefs(registry, for_test);
  PowerPrefs::RegisterUserProfilePrefs(registry);
  SessionControllerImpl::RegisterUserProfilePrefs(registry);
}

}  // namespace ash
