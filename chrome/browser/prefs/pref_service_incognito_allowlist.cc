// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/prefs/pref_service_incognito_allowlist.h"

#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/ukm/ukm_pref_names.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// List of keys that can be changed in the user prefs file by the incognito
// profile.
const char* const kPersistentPrefNames[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Accessibility preferences should be persisted if they are changed in
    // incognito mode.
    ash::prefs::kAccessibilityLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorDipSize,
    ash::prefs::kAccessibilityStickyKeysEnabled,
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    ash::prefs::kAccessibilityChromeVoxAutoRead,
    ash::prefs::kAccessibilityChromeVoxAnnounceDownloadNotifications,
    ash::prefs::kAccessibilityChromeVoxAnnounceRichTextAttributes,
    ash::prefs::kAccessibilityChromeVoxAudioStrategy,
    ash::prefs::kAccessibilityChromeVoxBrailleSideBySide,
    ash::prefs::kAccessibilityChromeVoxBrailleTable,
    ash::prefs::kAccessibilityChromeVoxBrailleTable6,
    ash::prefs::kAccessibilityChromeVoxBrailleTable8,
    ash::prefs::kAccessibilityChromeVoxBrailleTableType,
    ash::prefs::kAccessibilityChromeVoxBrailleWordWrap,
    ash::prefs::kAccessibilityChromeVoxCapitalStrategy,
    ash::prefs::kAccessibilityChromeVoxCapitalStrategyBackup,
    ash::prefs::kAccessibilityChromeVoxEnableBrailleLogging,
    ash::prefs::kAccessibilityChromeVoxEnableEarconLogging,
    ash::prefs::kAccessibilityChromeVoxEnableEventStreamLogging,
    ash::prefs::kAccessibilityChromeVoxEnableSpeechLogging,
    ash::prefs::kAccessibilityChromeVoxEventStreamFilters,
    ash::prefs::kAccessibilityChromeVoxLanguageSwitching,
    ash::prefs::kAccessibilityChromeVoxMenuBrailleCommands,
    ash::prefs::kAccessibilityChromeVoxNumberReadingStyle,
    ash::prefs::kAccessibilityChromeVoxPreferredBrailleDisplayAddress,
    ash::prefs::kAccessibilityChromeVoxPunctuationEcho,
    ash::prefs::kAccessibilityChromeVoxSmartStickyMode,
    ash::prefs::kAccessibilityChromeVoxSpeakTextUnderMouse,
    ash::prefs::kAccessibilityChromeVoxUsePitchChanges,
    ash::prefs::kAccessibilityChromeVoxUseVerboseMode,
    ash::prefs::kAccessibilityChromeVoxVirtualBrailleColumns,
    ash::prefs::kAccessibilityChromeVoxVirtualBrailleRows,
    ash::prefs::kAccessibilityChromeVoxVoiceName,
    ash::prefs::kAccessibilityColorCorrectionEnabled,
    ash::prefs::kAccessibilityColorVisionCorrectionAmount,
    ash::prefs::kAccessibilityColorVisionCorrectionType,
    ash::prefs::kAccessibilityReducedAnimationsEnabled,
    ash::prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted,
    ash::prefs::kAccessibilityFaceGazeEnabled,
    ash::prefs::kAccessibilityFaceGazeCursorSpeedUp,
    ash::prefs::kAccessibilityFaceGazeCursorSpeedDown,
    ash::prefs::kAccessibilityFaceGazeCursorSpeedLeft,
    ash::prefs::kAccessibilityFaceGazeCursorSpeedRight,
    ash::prefs::kAccessibilityFaceGazeCursorSmoothing,
    ash::prefs::kAccessibilityFaceGazeCursorUseAcceleration,
    ash::prefs::kAccessibilityFaceGazeGesturesToKeyCombos,
    ash::prefs::kAccessibilityFaceGazeGesturesToMacros,
    ash::prefs::kAccessibilityFaceGazeGesturesToConfidence,
    ash::prefs::kAccessibilityFaceGazeActionsEnabled,
    ash::prefs::kAccessibilityFaceGazeCursorControlEnabled,
    ash::prefs::kAccessibilityFaceGazeAdjustSpeedSeparately,
    ash::prefs::kAccessibilityFaceGazeVelocityThreshold,
    ash::prefs::kAccessibilityFlashNotificationsEnabled,
    ash::prefs::kAccessibilityFlashNotificationsColor,
    ash::prefs::kAccessibilityHighContrastEnabled,
    ash::prefs::kAccessibilityScreenMagnifierEnabled,
    ash::prefs::kAccessibilityScreenMagnifierFocusFollowingEnabled,
    ash::prefs::kAccessibilityMagnifierFollowsChromeVox,
    ash::prefs::kAccessibilityMagnifierFollowsSts,
    ash::prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
    ash::prefs::kAccessibilityScreenMagnifierScale,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityMonoAudioEnabled,
    ash::prefs::kAccessibilityAutoclickEnabled,
    ash::prefs::kAccessibilityAutoclickDelayMs,
    ash::prefs::kAccessibilityAutoclickEventType,
    ash::prefs::kAccessibilityAutoclickRevertToLeftClick,
    ash::prefs::kAccessibilityAutoclickStabilizePosition,
    ash::prefs::kAccessibilityAutoclickMovementThreshold,
    ash::prefs::kAccessibilityMouseKeysEnabled,
    ash::prefs::kAccessibilityMouseKeysAcceleration,
    ash::prefs::kAccessibilityMouseKeysMaxSpeed,
    ash::prefs::kAccessibilityMouseKeysUsePrimaryKeys,
    ash::prefs::kAccessibilityMouseKeysDominantHand,
    ash::prefs::kAccessibilityCaretHighlightEnabled,
    ash::prefs::kAccessibilityCaretBlinkInterval,
    ash::prefs::kAccessibilityCursorHighlightEnabled,
    ash::prefs::kAccessibilityCursorColorEnabled,
    ash::prefs::kAccessibilityCursorColor,
    ash::prefs::kAccessibilityFocusHighlightEnabled,
    ash::prefs::kAccessibilitySelectToSpeakEnabled,
    ash::prefs::kAccessibilitySwitchAccessEnabled,
    ash::prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes,
    ash::prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes,
    ash::prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes,
    ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled,
    ash::prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
    ash::prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
    ash::prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond,
    ash::prefs::kAccessibilityDictationEnabled,
    ash::prefs::kAccessibilityDictationLocale,
    ash::prefs::kDockedMagnifierEnabled,
    ash::prefs::kDockedMagnifierScale,
    ash::prefs::kDockedMagnifierScreenHeightDivisor,
    ash::prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
    ash::prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kShouldAlwaysShowAccessibilityMenu,
    ash::prefs::kAccessibilityDisableTrackpadEnabled,
    ash::prefs::kAccessibilityDisableTrackpadMode,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
    kAnimationPolicyAllowed,
    kAnimationPolicyOnce,
    kAnimationPolicyNone,
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_EXTENSIONS)
    prefs::kAnimationPolicy,
#endif

    // Bookmark preferences are common between incognito and regular mode.
    bookmarks::prefs::kBookmarkEditorExpandedNodes,
    bookmarks::prefs::kEditBookmarksEnabled,
    bookmarks::prefs::kManagedBookmarks,
    bookmarks::prefs::kManagedBookmarksFolderName,
    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
    bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
    bookmarks::prefs::kShowBookmarkBar,
#if BUILDFLAG(IS_ANDROID)
    prefs::kPartnerBookmarkMappings,
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    // Clipboard modification state is updated over all profiles.
    prefs::kClipboardLastModifiedTime,
#endif

    // Default browser bar's status is aggregated between regular and incognito
    // modes.
    prefs::kBrowserSuppressDefaultBrowserPrompt,
    prefs::kDefaultBrowserLastDeclined,
    prefs::kDefaultBrowserSettingEnabled,

    // Devtools preferences are stored cross profiles as they are not storing
    // user data and just keep debugging environment settings.
    prefs::kDevToolsAdbKey,
    prefs::kDevToolsAvailability,
    prefs::kDevToolsDiscoverUsbDevicesEnabled,
    prefs::kDevToolsEditedFiles,
    prefs::kDevToolsFileSystemPaths,
    prefs::kDevToolsPortForwardingEnabled,
    prefs::kDevToolsPortForwardingDefaultSet,
    prefs::kDevToolsPortForwardingConfig,
    prefs::kDevToolsPreferences,
    prefs::kDevToolsDiscoverTCPTargetsEnabled,
    prefs::kDevToolsTCPDiscoveryConfig,

#if BUILDFLAG(IS_WIN)
    // The total number of times that network profile warning is shown is
    // aggregated between regular and incognito modes.
    prefs::kNetworkProfileWarningsLeft,
#endif

    // Tab stats metrics are aggregated between regular and incognio mode.
    prefs::kTabStatsTotalTabCountMax,
    prefs::kTabStatsMaxTabsPerWindow,
    prefs::kTabStatsWindowCountMax,
    prefs::kTabStatsDailySample,

#if BUILDFLAG(IS_MAC)
    prefs::kShowFullscreenToolbar,
#endif

#if BUILDFLAG(IS_LINUX)
    // Toggleing custom frames affects all open windows in the profile, hence
    // should be written to the regular profile when changed in incognito mode.
    prefs::kUseCustomChromeFrame,
#endif

    // Although UKMs are not collected in incognito, theses preferences may be
    // changed by UMA/Sync/Unity consent, and need to be the same between
    // incognito and regular modes.
    ukm::prefs::kUkmClientId,
    ukm::prefs::kUkmUnsentLogStore,
    ukm::prefs::kUkmSessionId,

    // Cookie controls preference is, as in an initial release, surfaced only in
    // the incognito mode and therefore should be persisted between incognito
    // sessions.
    prefs::kCookieControlsMode,
};

}  // namespace

namespace prefs {

std::vector<const char*> GetIncognitoPersistentPrefsAllowlist() {
  std::vector<const char*> allowlist;
  allowlist.insert(allowlist.end(), kPersistentPrefNames,
                   kPersistentPrefNames + std::size(kPersistentPrefNames));
  return allowlist;
}

}  // namespace prefs
