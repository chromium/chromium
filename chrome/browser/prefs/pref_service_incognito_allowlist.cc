// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_incognito_allowlist.h"

#include <vector>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/ukm/ukm_pref_names.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// List of keys that can be changed in the user prefs file by the incognito
// profile.
const char* const kPersistentPrefNames[] = {
#if defined(OS_CHROMEOS)
    // Accessibility preferences should be persisted if they are changed in
    // incognito mode.
    ash::prefs::kAccessibilityLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorDipSize,
    ash::prefs::kAccessibilityStickyKeysEnabled,
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    ash::prefs::kAccessibilityHighContrastEnabled,
    ash::prefs::kAccessibilityScreenMagnifierCenterFocus,
    ash::prefs::kAccessibilityScreenMagnifierEnabled,
    ash::prefs::kAccessibilityScreenMagnifierFocusFollowingEnabled,
    ash::prefs::kAccessibilityScreenMagnifierScale,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityMonoAudioEnabled,
    ash::prefs::kAccessibilityAutoclickEnabled,
    ash::prefs::kAccessibilityAutoclickDelayMs,
    ash::prefs::kAccessibilityAutoclickEventType,
    ash::prefs::kAccessibilityAutoclickRevertToLeftClick,
    ash::prefs::kAccessibilityAutoclickStabilizePosition,
    ash::prefs::kAccessibilityAutoclickMovementThreshold,
    ash::prefs::kAccessibilityCaretHighlightEnabled,
    ash::prefs::kAccessibilityCursorHighlightEnabled,
    ash::prefs::kAccessibilityCursorColorEnabled,
    ash::prefs::kAccessibilityCursorColor,
    ash::prefs::kAccessibilityFocusHighlightEnabled,
    ash::prefs::kAccessibilitySelectToSpeakEnabled,
    ash::prefs::kAccessibilitySwitchAccessEnabled,
    ash::prefs::kAccessibilitySwitchAccessSelectKeyCodes,
    ash::prefs::kAccessibilitySwitchAccessSelectSetting,
    ash::prefs::kAccessibilitySwitchAccessNextKeyCodes,
    ash::prefs::kAccessibilitySwitchAccessNextSetting,
    ash::prefs::kAccessibilitySwitchAccessPreviousKeyCodes,
    ash::prefs::kAccessibilitySwitchAccessPreviousSetting,
    ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled,
    ash::prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
    ash::prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
    ash::prefs::kAccessibilityDictationEnabled,
    ash::prefs::kDockedMagnifierEnabled,
    ash::prefs::kDockedMagnifierScale,
    ash::prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
    ash::prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kShouldAlwaysShowAccessibilityMenu,
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_ANDROID)
    kAnimationPolicyAllowed,
    kAnimationPolicyOnce,
    kAnimationPolicyNone,
#endif  // !defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
    prefs::kPartnerBookmarkMappings,
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    // Clipboard modification state is updated over all profiles.
    prefs::kClipboardLastModifiedTime,
#endif

    // Default browser bar's status is aggregated between regular and incognito
    // modes.
    prefs::kBrowserSuppressDefaultBrowserPrompt,
    prefs::kDefaultBrowserLastDeclined,
    prefs::kDefaultBrowserSettingEnabled,
    prefs::kResetCheckDefaultBrowser,

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

#if defined(OS_WIN)
    // The total number of times that network profile warning is shown is
    // aggregated between regular and incognito modes.
    prefs::kNetworkProfileWarningsLeft,
#endif

    // Tab stats metrics are aggregated between regular and incognio mode.
    prefs::kTabStatsTotalTabCountMax,
    prefs::kTabStatsMaxTabsPerWindow,
    prefs::kTabStatsWindowCountMax,
    prefs::kTabStatsDailySample,

#if defined(OS_MAC)
    prefs::kShowFullscreenToolbar,
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    // Toggleing custom frames affects all open windows in the profile, hence
    // should be written to the regular profile when changed in incognito mode.
    prefs::kUseCustomChromeFrame,
#endif

    // Rappor preferences are not used in incognito mode, but they are written
    // in startup if they don't exist. So if the startup would be in incognito,
    // they need to be persisted.
    rappor::prefs::kRapporCohortSeed,
    rappor::prefs::kRapporSecret,

    // Reading list preferences are common between incognito and regular mode.
    reading_list::prefs::kReadingListHasUnseenEntries,

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
  std::vector<const char*> whitelist;
  whitelist.insert(whitelist.end(), kPersistentPrefNames,
                   kPersistentPrefNames + base::size(kPersistentPrefNames));
  return whitelist;
}

}  // namespace prefs
