// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/prefs/chrome_syncable_prefs_database.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "components/metrics/demographics/user_demographics.h"
#include "ui/chromeos/events/pref_names.h"
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/pref_names.h"
#endif

namespace browser_sync {

bool ChromeSyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  // Non-iOS specific list of syncable preferences.
  static const auto kChromeSyncablePrefsAllowlist = base::MakeFixedFlatSet<
      base::StringPiece>({
  // clang-format off
#if BUILDFLAG(IS_ANDROID)
        language::prefs::kAppLanguagePromptShown,
        translate::TranslatePrefs::kPrefExplicitLanguageAskShown,
        prefs::kContextualSearchEnabled,
        prefs::kContextualSearchWasFullyPrivacyEnabled,
        prefs::kAccessibilityImageLabelsEnabledAndroid,
        prefs::kAccessibilityImageLabelsOnlyOnWifi,
        prefs::kPromptForDownloadAndroid,
#else
        prefs::kAccessibilityReadAnythingFontName,
        prefs::kAccessibilityReadAnythingFontScale,
        prefs::kAccessibilityReadAnythingColorInfo,
        prefs::kAccessibilityReadAnythingLineSpacing,
        prefs::kAccessibilityReadAnythingLetterSpacing,
        prefs::kLensRegionSearchEnabled,
        prefs::kHatsSurveyMetadata,
        prefs::kHomePage,
        prefs::kHomePageIsNewTabPage,
        prefs::kNtpCustomBackgroundDict,
        prefs::kNtpDisabledModules,
        prefs::kNtpModulesOrder,
        prefs::kNtpModulesVisible,
        prefs::kNtpModulesShownCount,
        prefs::kNtpModulesFirstShownTime,
        prefs::kNtpModulesFreVisible,
        prefs::kNtpCustomizeChromeButtonOpenCount,
        prefs::kLiveCaptionBubbleExpanded,
        prefs::kLiveCaptionBubblePinned,
        prefs::kLiveCaptionEnabled,
        prefs::kLiveCaptionLanguageCode,
        prefs::kLiveCaptionMediaFoundationRendererErrorSilenced,
        prefs::kShowHomeButton,
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_EXTENSIONS)
        extensions::pref_names::kPinnedExtensions,
#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
        prefs::kSupervisedUserApprovedExtensions,
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_WIN)
        prefs::kIsDefaultPageColorsOnHighContrast,
#endif
#if BUILDFLAG(IS_MAC)
        prefs::kShowFullscreenToolbar,
        prefs::kAllowJavascriptAppleEvents,
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
        ash::full_restore::kRestoreAppsAndPagesPrefName,
        ash::prefs::kAccessibilityAutoclickDelayMs,
        ash::prefs::kAccessibilityAutoclickEventType,
        ash::prefs::kAccessibilityAutoclickMenuPosition,
        ash::prefs::kAccessibilityAutoclickMovementThreshold,
        ash::prefs::kAccessibilityAutoclickRevertToLeftClick,
        ash::prefs::kAccessibilityAutoclickStabilizePosition,
        ash::prefs::kAccessibilityCursorColor,
        ash::prefs::kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed,
        ash::prefs::kAccessibilityFloatingMenuPosition,
        ash::prefs::kAccessibilityGreyscaleAmount,
        ash::prefs::kAccessibilityHueRotationAmount,
        ash::prefs::kAccessibilitySaturationAmount,
        ash::prefs::kAccessibilityScreenMagnifierCenterFocus,
        ash::prefs::kAccessibilityScreenMagnifierFocusFollowingEnabled,
        ash::prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
        ash::prefs::kAccessibilitySelectToSpeakBackgroundShading,
        ash::prefs::kAccessibilitySelectToSpeakEnhancedNetworkVoices,
        ash::prefs::kAccessibilitySelectToSpeakEnhancedVoiceName,
        ash::prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown,
        ash::prefs::kAccessibilitySelectToSpeakHighlightColor,
        ash::prefs::kAccessibilitySelectToSpeakNavigationControls,
        ash::prefs::kAccessibilitySelectToSpeakVoiceName,
        ash::prefs::kAccessibilitySelectToSpeakVoiceSwitching,
        ash::prefs::kAccessibilitySelectToSpeakWordHighlight,
        ash::prefs::kAccessibilitySepiaAmount,
        ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled,
        ash::prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
        ash::prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
        ash::prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes,
        ash::prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond,
        ash::prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes,
        ash::prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes,
        ash::prefs::kAppNotificationBadgingEnabled,
        ash::prefs::kEnableAutoScreenLock,
        ash::prefs::kEnableStylusTools,
        ash::prefs::kFilesAppFolderShortcuts,
        ash::prefs::kFilesAppTrashEnabled,
        ash::prefs::kFilesAppUIPrefsMigrated,
        ash::prefs::kLaunchPaletteOnEjectEvent,
        ash::prefs::kLauncherContinueSectionHidden,
        ash::prefs::kLauncherFeedbackOnContinueSectionSent,
        ash::prefs::kLauncherResultEverLaunched,
        ash::prefs::kMessageCenterLockScreenMode,
        ash::prefs::kMouseAcceleration,
        ash::prefs::kMouseReverseScroll,
        ash::prefs::kMouseScrollAcceleration,
        ash::prefs::kMouseScrollSensitivity,
        ash::prefs::kMouseSensitivity,
        ash::prefs::kNaturalScroll,
        ash::prefs::kOobeMarketingOptInChoice,
        ash::prefs::kOobeMarketingOptInScreenFinished,
        ash::prefs::kPointingStickAcceleration,
        ash::prefs::kPointingStickSensitivity,
        ash::prefs::kPowerAdaptiveChargingEnabled,
        ash::prefs::kPowerAdaptiveChargingNudgeShown,
        ash::prefs::kPrimaryMouseButtonRight,
        ash::prefs::kPrimaryPointingStickButtonRight,
        ash::prefs::kProjectorAnnotatorLastUsedMarkerColor,
        ash::prefs::kProjectorCreationFlowEnabled,
        ash::prefs::kProjectorCreationFlowLanguage,
        ash::prefs::kProjectorGalleryOnboardingShowCount,
        ash::prefs::kProjectorViewerOnboardingShowCount,
        ash::prefs::kShelfAlignment,
        ash::prefs::kShelfAutoHideBehavior,
        ash::prefs::kSuggestedContentEnabled,
        ash::prefs::kSyncableWallpaperInfo,
        ash::prefs::kTapDraggingEnabled,
        ash::prefs::kTapToClickEnabled,
        ash::prefs::kTouchpadAcceleration,
        ash::prefs::kTouchpadHapticClickSensitivity,
        ash::prefs::kTouchpadHapticFeedback,
        ash::prefs::kTouchpadScrollAcceleration,
        ash::prefs::kTouchpadScrollSensitivity,
        ash::prefs::kTouchpadSensitivity,
        ash::prefs::kXkbAutoRepeatDelay,
        ash::prefs::kXkbAutoRepeatEnabled,
        ash::prefs::kXkbAutoRepeatInterval,
        ash::tether::prefs::kMostRecentConnectTetheringResponderIds,
        ash::tether::prefs::kMostRecentTetherAvailablilityResponderIds,
        guest_os::prefs::kGuestOsTerminalSettings,
        language::prefs::kPreferredLanguagesSyncable,
        metrics::kSyncOsDemographicsPrefName,
        prefs::kAppListPreferredOrder,
        prefs::kChromeOSReleaseNotesVersion,
        prefs::kLanguageEnabledImesSyncable,
        prefs::kLanguagePreloadEnginesSyncable,
        prefs::kLanguageRemapAltKeyTo,
        prefs::kLanguageRemapAssistantKeyTo,
        prefs::kLanguageRemapBackspaceKeyTo,
        prefs::kLanguageRemapCapsLockKeyTo,
        prefs::kLanguageRemapControlKeyTo,
        prefs::kLanguageRemapEscapeKeyTo,
        prefs::kLanguageRemapExternalCommandKeyTo,
        prefs::kLanguageRemapExternalMetaKeyTo,
        prefs::kLanguageRemapSearchKeyTo,
        prefs::kMultiProfileNeverShowIntro,
        prefs::kMultiProfileWarningShowDismissed,
        prefs::kOfficeSetupComplete,
        prefs::kResolveTimezoneByGeolocationMethod,
        prefs::kResolveTimezoneByGeolocationMigratedToMethod,
        prefs::kShelfDefaultPinLayoutRolls,
        prefs::kTextToSpeechLangToVoiceName,
        prefs::kTextToSpeechPitch,
        prefs::kTextToSpeechRate,
        prefs::kTextToSpeechVolume,
        prefs::kUse24HourClock,
        prefs::kUserPrintersAllowed,
        proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
        // This is not exposed in a header.
        // TODO(crbug.com/1420978): Declare this in the corresponding header.
        "user_image_info",
        // CI complains about /components/drive:drive not being part of deps
        // even though it is. Putting these as literals.
        // TODO(crbug.com/1401271): Investigate this failure.
        "gdata.disabled",
        "gdata.cellular.disabled",
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        embedder_support::kAlternateErrorPagesEnabled,
        performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
        prefs::kAccessibilityImageLabelsEnabled,
        prefs::kAccessibilityImageLabelsOptInAccepted,
        prefs::kAccessibilityPdfOcrAlwaysActive,
        prefs::kApplyPageColorsOnlyOnIncreasedContrast,
        prefs::kDefaultCharset,
        prefs::kDefaultTasksByMimeType,
        prefs::kDefaultTasksBySuffix,
        prefs::kDevToolsSyncPreferences,
        prefs::kDevToolsSyncedPreferencesSyncEnabled,
        prefs::kDownloadBubbleIphSuppression,
        prefs::kEnableDoNotTrack,
        prefs::kExtensionCommands,
        prefs::kExtensionsUIDeveloperMode,
        prefs::kHttpsOnlyModeEnabled,
        prefs::kLiveTranslateEnabled,
        prefs::kLiveTranslateTargetLanguageCode,
        prefs::kNetworkEasterEggHighScore,
        prefs::kNetworkPredictionOptions,
        prefs::kNetworkQualities,
        prefs::kNtpAppPageNames,
        prefs::kPageColors,
        prefs::kPerformanceTracingEnabled,
        prefs::kPluginsAlwaysOpenPdfExternally,
        prefs::kPrivacySandboxApisEnabled,
        prefs::kPrivacySandboxFirstPartySetsEnabled,
        prefs::kPrivacySandboxManuallyControlled,
        prefs::kPromptForDownload,
        prefs::kProtectedContentDefault,
        prefs::kRestoreOnStartup,
        prefs::kSearchSuggestEnabled,
        prefs::kSharingVapidKey,
        prefs::kURLsToRestoreOnStartup,
        spellcheck::prefs::kSpellCheckEnable,
        // The following prefs are constructed from a prefix in
        // website_settings_info and are registered in
        // content_settings_registry.
        "profile.content_settings.exceptions.anti_abuse",
        "profile.default_content_setting_values.anti_abuse",
        "profile.content_settings.exceptions.automatic_downloads",
        "profile.default_content_setting_values.automatic_downloads",
        "profile.content_settings.exceptions.cookies",
        "profile.default_content_setting_values.cookies",
        "profile.content_settings.exceptions.get_display_media_set_select_"
          "all_screens",
        "profile.default_content_setting_values.get_display_media_set_"
          "select_all_screens",
        "profile.content_settings.exceptions.images",
        "profile.default_content_setting_values.images",
        "profile.content_settings.exceptions.javascript",
        "profile.default_content_setting_values.javascript",
        "profile.content_settings.exceptions.local_fonts",
        "profile.default_content_setting_values.local_fonts",
        "profile.content_settings.exceptions.mouselock",
        "profile.default_content_setting_values.mouselock",
        "profile.content_settings.exceptions.popups",
        "profile.default_content_setting_values.popups",
        "profile.default_content_setting_values.window_placement",
        "profile.content_settings.exceptions.window_placement",
        // This is not exposed in a header.
        // TODO(crbug.com/1420978): Declare this in the corresponding header.
        "webauthn.cablev2_pairings"
  // clang-format off
  });
  return kChromeSyncablePrefsAllowlist.count(pref_name) ||
         // Also check if `pref_name` is part of the common set of syncable
         // preferences.
         common_syncable_prefs_database_.IsPreferenceSyncable(pref_name);
}
} // namespace browser_sync
