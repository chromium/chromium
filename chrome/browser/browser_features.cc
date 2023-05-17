// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/net/system_network_context_manager.h"
#endif

namespace features {

// Enables using the ClosedTabCache to instantly restore recently closed tabs
// using the "Reopen Closed Tab" button.
BASE_FEATURE(kClosedTabCache,
             "ClosedTabCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Destroy profiles when their last browser window is closed, instead of when
// the browser exits.
// On Lacros the feature is enabled only for secondary profiles, check the
// implementation of `ProfileManager::ProfileInfo::FromUnownedProfile()`.
BASE_FEATURE(kDestroyProfileOnBrowserClose,
             "DestroyProfileOnBrowserClose",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// DestroyProfileOnBrowserClose only covers deleting regular (non-System)
// Profiles. This flags lets us destroy the System Profile, as well.
BASE_FEATURE(kDestroySystemProfiles,
             "DestroySystemProfiles",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Let DevTools front-end talk to the target of type "tab" rather than
// "frame" when inspecting a WebContents.
BASE_FEATURE(kDevToolsTabTarget,
             "DevToolsTabTarget",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Nukes profile directory before creating a new profile using
// ProfileManager::CreateMultiProfileAsync().
BASE_FEATURE(kNukeProfileBeforeCreateMultiAsync,
             "NukeProfileBeforeCreateMultiAsync",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables executing the browser commands sent by the NTP promos.
BASE_FEATURE(kPromoBrowserCommands,
             "PromoBrowserCommands",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Parameter name for the promo browser command ID provided along with
// kPromoBrowserCommands.
// The value of this parameter should be parsable as an unsigned integer and
// should map to one of the browser commands specified in:
// ui/webui/resources/js/browser_command/browser_command.mojom
const char kBrowserCommandIdParam[] = "BrowserCommandIdParam";

#if BUILDFLAG(IS_MAC)
// Enables integration with the macOS feature Universal Links.
BASE_FEATURE(kEnableUniveralLinks,
             "EnableUniveralLinks",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables reading and writing PWA notification permissions from quick settings
// menu.
BASE_FEATURE(kQuickSettingsPWANotifications,
             "QuickSettingsPWA",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Enables being able to zoom a web page by double tapping in Chrome OS tablet
// mode.
BASE_FEATURE(kDoubleTapToZoomInTabletMode,
             "DoubleTapToZoomInTabletMode",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
// Adds an item to the context menu that copies a link to the page with the
// selected text highlighted.
BASE_FEATURE(kCopyLinkToText,
             "CopyLinkToText",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds a "Snooze" action to mute notifications during screen sharing sessions.
BASE_FEATURE(kMuteNotificationSnoozeAction,
             "MuteNotificationSnoozeAction",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Gates sandboxed iframe navigation toward external protocol behind any of:
// - allow-top-navigation
// - allow-top-navigation-to-custom-protocols
// - allow-top-navigation-with-user-gesture (+ user gesture)
// - allow-popups
//
// Motivation:
// Developers are surprised that a sandboxed iframe can navigate and/or
// redirect the user toward an external application.
// General iframe navigation in sandboxed iframe are not blocked normally,
// because they stay within the iframe. However they can be seen as a popup or
// a top-level navigation when it leads to opening an external application. In
// this case, it makes sense to extend the scope of sandbox flags, to block
// malvertising.
//
// Implementation bug: https://crbug.com/1253379
// I2S: https://groups.google.com/a/chromium.org/g/blink-dev/c/-t-f7I6VvOI
//
// Enabled in M103. Flag to be removed in M106
BASE_FEATURE(kSandboxExternalProtocolBlocked,
             "SandboxExternalProtocolBlocked",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enabled in M100. Flag to be removed in M106
BASE_FEATURE(kSandboxExternalProtocolBlockedWarning,
             "SandboxExternalProtocolBlockedWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables migration of the network context data from `unsandboxed_data_path` to
// `data_path`. See the explanation in network_context.mojom.
BASE_FEATURE(kTriggerNetworkDataMigration,
             "TriggerNetworkDataMigration",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_CHROMEOS)
// If enabled, a blue border is drawn around shared tabs on ChromeOS.
// If disabled, the blue border is not used on ChromeOS.
//
// Motivation:
//  The blue border behavior used to cause problems on ChromeOS - see
//  crbug.com/1320262 for Ash (fixed) and crbug.com/1030925 for Lacros
//  (relatively old bug - we would like to observe whether it's still
//  there). This flag is introduced as means of disabling this feature in case
//  of possible future regressions.
//
// TODO(crbug.com/1251999): Remove this flag once we confirm that blue border
// works fine on ChromeOS.
//
// b/279051234: We suspect the tab sharing blue border may cause a bad issue
// on ChromeOS where a window can not be interacted at all. Disable the feature
// on ChromeOS.
BASE_FEATURE(kTabCaptureBlueBorderCrOS,
             "TabCaptureBlueBorderCrOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables runtime detection of USB devices which provide a WebUSB landing page
// descriptor.
BASE_FEATURE(kWebUsbDeviceDetection,
             "WebUsbDeviceDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enables Certificate Transparency on Android.
BASE_FEATURE(kCertificateTransparencyAndroid,
             "CertificateTransparencyAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kLargeFaviconFromGoogle,
             "LargeFaviconFromGoogle",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kLargeFaviconFromGoogleSizeInDip{
    &kLargeFaviconFromGoogle, "favicon_size_in_dip", 128};

// Controls whether the static key pinning list can be updated via component
// updater.
BASE_FEATURE(kKeyPinningComponentUpdater,
             "KeyPinningComponentUpdater",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When this feature is enabled, the network service will restart unsandboxed if
// a previous attempt to launch it sandboxed failed.
BASE_FEATURE(kRestartNetworkServiceUnsandboxedForFailedLaunch,
             "RestartNetworkServiceUnsandboxedForFailedLaunch",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// When this feature is enabled, metrics are gathered regarding the performance
// and reliability of app-bound encryption primitives on a background thread.
BASE_FEATURE(kAppBoundEncryptionMetrics,
             "AppBoundEncryptionMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables locking the cookie database for profiles.
// TODO(crbug.com/1430226): Remove after fully launched.
BASE_FEATURE(kLockProfileCookieDatabase,
             "LockProfileCookieDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables showing the email of the flex org admin that setup CBCM in the
// management disclosures.
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kFlexOrgManagementDisclosure,
             "FlexOrgManagementDisclosure",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kFlexOrgManagementDisclosure,
             "FlexOrgManagementDisclosure",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Enables usage of the FedCM API without third party cookies at the same time.
BASE_FEATURE(kFedCmWithoutThirdPartyCookies,
             "FedCmWithoutThirdPartyCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Incoming Call Notifications scenario. When created by an
// installed origin, an incoming call notification should have increased
// priority, colored buttons, a ringtone, and a default "close" button.
// Otherwise, if the origin is not installed, it should behave like the default
// notifications, but with the added "Close" button. See
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/Notifications/notifications_actions_customization.md
BASE_FEATURE(kIncomingCallNotifications,
             "IncomingCallNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables omnibox trigger prerendering.
BASE_FEATURE(kOmniboxTriggerForPrerender2,
             "OmniboxTriggerForPrerender2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables bookmark trigger prerendering.
BASE_FEATURE(kBookmarkTriggerForPrerender2,
             "BookmarkTriggerForPrerender2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSupportSearchSuggestionForPrerender2,
             "SupportSearchSuggestionForPrerender2",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<SearchSuggestionPrerenderImplementationType>::Option
    search_suggestion_implementation_types[] = {
        {SearchSuggestionPrerenderImplementationType::kUsePrefetch,
         "use_prefetch"},
        {SearchSuggestionPrerenderImplementationType::kIgnorePrefetch,
         "ignore_prefetch"}};
const base::FeatureParam<SearchSuggestionPrerenderImplementationType>
    kSearchSuggestionPrerenderImplementationTypeParam{
        &kSupportSearchSuggestionForPrerender2, "implementation_type",
        SearchSuggestionPrerenderImplementationType::kIgnorePrefetch,
        &search_suggestion_implementation_types};

const base::FeatureParam<SearchPreloadShareableCacheType>::Option
    search_preload_shareable_cache_types[] = {
        {SearchPreloadShareableCacheType::kEnabled, "enabled"},
        {SearchPreloadShareableCacheType::kDisabled, "disabled"}};
const base::FeatureParam<SearchPreloadShareableCacheType>
    kSearchPreloadShareableCacheTypeParam{
        &kSupportSearchSuggestionForPrerender2, "shareable_cache",
        SearchPreloadShareableCacheType::kEnabled,
        &search_preload_shareable_cache_types};

BASE_FEATURE(kAutocompleteActionPredictorConfidenceCutoff,
             "AutocompleteActionPredictorConfidenceCutoff",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables omnibox trigger no state prefetch. Only one of
// kOmniboxTriggerForPrerender2 or kOmniboxTriggerForNoStatePrefetch can be
// enabled in the experiment. If both are enabled, only
// kOmniboxTriggerForPrerender2 takes effect.
// TODO(crbug.com/1267731): Remove this flag once the experiments are completed.
BASE_FEATURE(kOmniboxTriggerForNoStatePrefetch,
             "OmniboxTriggerForNoStatePrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
