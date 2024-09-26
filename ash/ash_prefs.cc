// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_prefs.h"

#include "ash/accelerators/accelerator_prefs.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/accelerators/system_shortcut_behavior_policy.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/managed/screensaver_images_policy_handler.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_education_controller.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/display/display_prefs.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/edusumer/graduation_prefs.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/management_disclosure_field_trial.h"
#include "ash/media/media_controller_impl.h"
#include "ash/metrics/feature_discovery_duration_reporter_impl.h"
#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/views/picker_feature_tour.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/quick_pair/feature_status_tracker/scanning_enabled_provider.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/session/fullscreen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/bluetooth/bluetooth_device_status_ui_handler.h"
#include "ash/system/brightness/brightness_controller_chromeos.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/camera/autozoom_nudge_controller.h"
#include "ash/system/camera/camera_app_prefs.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/hotspot/hotspot_info_cache.h"
#include "ash/system/human_presence/snooping_protection_controller.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"
#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/keyboard_modifier_metrics_recorder.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"
#include "ash/system/mahi/mahi_nudge_controller.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/network/cellular_setup_notifier.h"
#include "ash/system/network/vpn_detailed_view.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"
#include "ash/system/phonehub/onboarding_nudge_controller.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/power_notification_controller.h"
#include "ash/system/power/power_prefs.h"
#include "ash/system/power/power_sounds_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/user_education/user_education_controller.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/wallpaper_daily_refresh_scheduler.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/wallpaper/wallpaper_time_of_day_scheduler.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/float/tablet_mode_tuck_education.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_privacy_nudge_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {

// Registers prefs whose default values are same in user and signin prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry,
                          std::string_view country,
                          bool for_test) {
  AcceleratorPrefs::RegisterProfilePrefs(registry);
  AccessibilityController::RegisterProfilePrefs(registry);
  AppListControllerImpl::RegisterProfilePrefs(registry);
  AppListNudgeController::RegisterProfilePrefs(registry);
  AshAcceleratorConfiguration::RegisterProfilePrefs(registry);
  AssistantControllerImpl::RegisterProfilePrefs(registry);
  AutozoomControllerImpl::RegisterProfilePrefs(registry);
  AutozoomNudgeController::RegisterProfilePrefs(registry);
  AmbientController::RegisterProfilePrefs(registry);
  BirchBarController::RegisterProfilePrefs(registry);
  BirchItem::RegisterProfilePrefs(registry);
  BirchModel::RegisterProfilePrefs(registry);
  BirchPrivacyNudgeController::RegisterProfilePrefs(registry);
  CalendarController::RegisterProfilePrefs(registry);
  camera_app_prefs::RegisterProfilePrefs(registry);
  CameraEffectsController::RegisterProfilePrefs(registry);
  CaptureModeController::RegisterProfilePrefs(registry);
  CaptureModeEducationController::RegisterProfilePrefs(registry);
  CellularSetupNotifier::RegisterProfilePrefs(registry);
  chromeos::MultitaskMenuNudgeController::RegisterProfilePrefs(registry);
  contextual_tooltip::RegisterProfilePrefs(registry);
  ClipboardHistoryControllerImpl::RegisterProfilePrefs(registry);
  ColorPaletteController::RegisterPrefs(registry);
  DarkLightModeControllerImpl::RegisterProfilePrefs(registry);
  desks_restore_util::RegisterProfilePrefs(registry);
  saved_desk_util::RegisterProfilePrefs(registry);
  window_util::RegisterProfilePrefs(registry);
  DockedMagnifierController::RegisterProfilePrefs(registry);
  FeatureDiscoveryDurationReporterImpl::RegisterProfilePrefs(registry);
  FocusModeController::RegisterProfilePrefs(registry);
  FullscreenController::RegisterProfilePrefs(registry);
  GameDashboardController::RegisterProfilePrefs(registry);
  GeolocationController::RegisterProfilePrefs(registry);
  GlanceablesController::RegisterUserProfilePrefs(registry);
  graduation_prefs::RegisterProfilePrefs(registry);
  holding_space_prefs::RegisterProfilePrefs(registry);
  HotspotInfoCache::RegisterProfilePrefs(registry);
  InputDeviceSettingsControllerImpl::RegisterProfilePrefs(registry);
  InputDeviceSettingsNotificationController::RegisterProfilePrefs(registry);
  InputDeviceTracker::RegisterProfilePrefs(registry);
  LoginScreenController::RegisterProfilePrefs(registry, for_test);
  LogoutButtonTray::RegisterProfilePrefs(registry);
  LogoutConfirmationController::RegisterProfilePrefs(registry);
  KeyboardBacklightColorController::RegisterPrefs(registry);
  KeyboardControllerImpl::RegisterProfilePrefs(registry, country);
  KeyboardModifierMetricsRecorder::RegisterProfilePrefs(registry, for_test);
  MahiNudgeController::RegisterProfilePrefs(registry);
  MediaControllerImpl::RegisterProfilePrefs(registry);
  MessageCenterController::RegisterProfilePrefs(registry);
  NightLightControllerImpl::RegisterProfilePrefs(registry);
  OnboardingNudgeController::RegisterProfilePrefs(registry);
  PaletteTray::RegisterProfilePrefs(registry);
  PaletteWelcomeBubble::RegisterProfilePrefs(registry);
  PickerFeatureTour::RegisterProfilePrefs(registry);
  PickerSessionMetrics::RegisterProfilePrefs(registry);
  PciePeripheralNotificationController::RegisterProfilePrefs(registry);
  PrivacyHubController::RegisterProfilePrefs(registry);
  PrivacyScreenController::RegisterProfilePrefs(registry);
  ProjectorControllerImpl::RegisterProfilePrefs(registry);
  quick_pair::Mediator::RegisterProfilePrefs(registry);
  RegisterSystemShortcutBehaviorProfilePrefs(registry);
  ScreensaverImagesPolicyHandler::RegisterPrefs(registry);
  SeaPenWallpaperManager::RegisterProfilePrefs(registry);
  ShelfController::RegisterProfilePrefs(registry);
  SnoopingProtectionController::RegisterProfilePrefs(registry);
  system::BrightnessControllerChromeos::RegisterProfilePrefs(registry);
  KeyboardBrightnessController::RegisterProfilePrefs(registry);
  TabletModeTuckEducation::RegisterProfilePrefs(registry);
  TouchDevicesController::RegisterProfilePrefs(registry, for_test);
  UserEducationController::RegisterProfilePrefs(registry);
  MediaTray::RegisterProfilePrefs(registry);
  UsbPeripheralNotificationController::RegisterProfilePrefs(registry);
  VideoConferenceTrayController::RegisterProfilePrefs(registry);
  VpnDetailedView::RegisterProfilePrefs(registry);
  WallpaperDailyRefreshScheduler::RegisterProfilePrefs(registry);
  WallpaperTimeOfDayScheduler::RegisterProfilePrefs(registry);
  WallpaperPrefManager::RegisterProfilePrefs(registry);
  WindowCycleController::RegisterProfilePrefs(registry);
  growth::CampaignsManager::RegisterProfilePrefs(registry);

  // Provide prefs registered in the browser for ash_unittests.
  if (for_test) {
    assistant::prefs::RegisterProfilePrefs(registry);
    quick_answers::prefs::RegisterProfilePrefs(registry);
    registry->RegisterBooleanPref(prefs::kMouseReverseScroll, false);
    registry->RegisterBooleanPref(prefs::kSendFunctionKeys, false);
    registry->RegisterBooleanPref(prefs::kSuggestedContentEnabled, true);
    registry->RegisterBooleanPref(prefs::kMagicBoostEnabled, true);
    registry->RegisterBooleanPref(prefs::kHmrEnabled, true);
    registry->RegisterBooleanPref(prefs::kHmrFeedbackAllowed, true);
    registry->RegisterBooleanPref(prefs::kOrcaEnabled, true);
    registry->RegisterBooleanPref(prefs::kOrcaFeedbackEnabled, true);
    registry->RegisterBooleanPref(::prefs::kLiveCaptionEnabled, false);
    registry->RegisterListPref(
        chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList);
    registry->RegisterStringPref(::prefs::kLiveCaptionLanguageCode,
                                 speech::kUsEnglishLocale);
    registry->RegisterStringPref(language::prefs::kApplicationLocale,
                                 std::string());
    registry->RegisterStringPref(language::prefs::kPreferredLanguages,
                                 std::string());
    registry->RegisterIntegerPref(prefs::kAltEventRemappedToRightClick, 0);
    registry->RegisterIntegerPref(
        prefs::kHMRConsentStatus,
        base::to_underlying(chromeos::HMRConsentStatus::kUnset));
    registry->RegisterIntegerPref(prefs::kHMRConsentWindowDismissCount, 0);
    registry->RegisterIntegerPref(prefs::kSearchEventRemappedToRightClick, 0);
    registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackDelete, 0);
    registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackEnd, 0);
    registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackHome, 0);
    registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackPageUp, 0);
    registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackPageDown, 0);
    registry->RegisterBooleanPref(prefs::kShowInformedRestoreOnboarding, false);
    registry->RegisterIntegerPref(prefs::kInformedRestoreNudgeShownCount, 0);
    registry->RegisterTimePref(prefs::kInformedRestoreNudgeLastShown,
                               base::Time());
  }
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry, bool for_test) {
  PaletteTray::RegisterLocalStatePrefs(registry);
  WallpaperPrefManager::RegisterLocalStatePrefs(registry);
  ColorPaletteController::RegisterLocalStatePrefs(registry);
  DetachableBaseHandler::RegisterPrefs(registry);
  PowerPrefs::RegisterLocalStatePrefs(registry);
  PrivacyHubController::RegisterLocalStatePrefs(registry);
  DisplayPrefs::RegisterLocalStatePrefs(registry);
  LoginExpandedPublicAccountView::RegisterLocalStatePrefs(registry);
  LockStateController::RegisterPrefs(registry);
  quick_pair::Mediator::RegisterLocalStatePrefs(registry);
  QuickSettingsFooter::RegisterLocalStatePrefs(registry);
  KeyboardBacklightColorController::RegisterPrefs(registry);
  BatterySaverController::RegisterLocalStatePrefs(registry);
  PowerSoundsController::RegisterLocalStatePrefs(registry);
  PowerNotificationController::RegisterLocalStatePrefs(registry);
  quick_pair::ScanningEnabledProvider::RegisterLocalStatePrefs(registry);
  InputDeviceSettingsMetadataManager::RegisterLocalStatePrefs(registry);
  BluetoothDeviceStatusUiHandler::RegisterLocalStatePrefs(registry);
  management_disclosure_field_trial::RegisterLocalStatePrefs(registry);

  if (for_test) {
    registry->RegisterBooleanPref(prefs::kOwnerPrimaryMouseButtonRight, false);
    user_manager::KnownUser::RegisterPrefs(registry);
  }
}

void RegisterSigninProfilePrefs(PrefRegistrySimple* registry,
                                std::string_view country,
                                bool for_test) {
  RegisterProfilePrefs(registry, country, for_test);
  PowerPrefs::RegisterSigninProfilePrefs(registry);
}

void RegisterUserProfilePrefs(PrefRegistrySimple* registry,
                              std::string_view country,
                              bool for_test) {
  RegisterProfilePrefs(registry, country, for_test);
  PowerPrefs::RegisterUserProfilePrefs(registry);
  SessionControllerImpl::RegisterUserProfilePrefs(registry);
}

}  // namespace ash
