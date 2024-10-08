// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"

#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/net/system_network_context_manager.h"
#endif

namespace features {

#if BUILDFLAG(IS_ANDROID)
// Kill switch for allowing TWAs to autoplay with sound without requiring a user
// gesture to unlock, for parity with PWAs.
BASE_FEATURE(kAllowUnmutedAutoplayForTWA,
             "AllowUnmutedAutoplayForTWA",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// This is used to enable an experiment for modifying confidence cutoff of
// prerender and preconnect for autocomplete action predictor.
BASE_FEATURE(kAutocompleteActionPredictorConfidenceCutoff,
             "AutocompleteActionPredictorConfidenceCutoff",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is used to enable an experiment for the bookmarks tree view in the
// side panel, providing users with a hierarchical view of their bookmarks.
BASE_FEATURE(kBookmarksTreeView,
             "BookmarksTreeView",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This flag is used for enabling Bookmark triggered prerendering. See
// crbug.com/1422819 for more details of Bookmark triggered prerendering.
BASE_FEATURE(kBookmarkTriggerForPrerender2,
             "BookmarkTriggerForPrerender2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Certificate Transparency on Desktop and Android Browser (CT is
// disabled in Android Webview, see aw_browser_context.cc).
// Enabling CT enforcement requires maintaining a log policy, and the ability to
// update the list of accepted logs. Embedders who are planning to enable this
// should first reach out to chrome-certificate-transparency@google.com.
// On builds where CT is enabled, this flag is also used as an emergency kill
// switch.
BASE_FEATURE(kCertificateTransparencyAskBeforeEnabling,
             "CertificateTransparencyAskBeforeEnabling",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Enables using network time for certificate verification. If enabled, network
// time will be used to verify certificate validity, however certificates that
// fail to validate with network time will fall back to the system time.
// This has no effect if the network_time::kNetworkTimeServiceQuerying flag is
// disabled, or the BrowserNetworkTimeQueriesEnabled policy is set to false.
BASE_FEATURE(kCertVerificationNetworkTime,
             "CertVerificationNetworkTime",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using the ClosedTabCache to instantly restore recently closed tabs
// using the "Reopen Closed Tab" button.
BASE_FEATURE(kClosedTabCache,
             "ClosedTabCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX)
// Enables usage of os_crypt_async::SecretPortalKeyProvider.  Once
// `kSecretPortalKeyProviderUseForEncryption` is enabled, this flag cannot be
// disabled without losing data.
BASE_FEATURE(kDbusSecretPortal,
             "DbusSecretPortal",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

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

#if BUILDFLAG(IS_CHROMEOS)
// Enables being able to zoom a web page by double tapping in Chrome OS tablet
// mode.
BASE_FEATURE(kDoubleTapToZoomInTabletMode,
             "DoubleTapToZoomInTabletMode",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// When this feature is enabled, the App-Bound encryption provider is used as
// the default encryption provider.
BASE_FEATURE(kUseAppBoundEncryptionProviderForEncryption,
             "UseAppBoundEncryptionProviderForEncryption",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// Enables showing the email of the flex org admin that setup CBCM in the
// management disclosures.
BASE_FEATURE(kFlexOrgManagementDisclosure,
             "FlexOrgManagementDisclosure",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Enables the Incoming Call Notifications scenario. When created by an
// installed origin, an incoming call notification should have increased
// priority, colored buttons, a ringtone, and a default "close" button.
// Otherwise, if the origin is not installed, it should behave like the default
// notifications, but with the added "Close" button. See
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/Notifications/notifications_actions_customization.md
BASE_FEATURE(kIncomingCallNotifications,
             "IncomingCallNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the static key pinning list can be updated via component
// updater.
BASE_FEATURE(kKeyPinningComponentUpdater,
             "KeyPinningComponentUpdater",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Enables locking the cookie database for profiles.
// TODO(crbug.com/40901624): Remove after fully launched.
BASE_FEATURE(kLockProfileCookieDatabase,
             "LockProfileCookieDatabase",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
// Adds a "Snooze" action to mute notifications during screen sharing sessions.
BASE_FEATURE(kMuteNotificationSnoozeAction,
             "MuteNotificationSnoozeAction",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// This feature enables monitoring of first-party network requests in order to
// find possible violations. Example: A Chrome policy is set to disabled but the
// network request controlled by that policy is observed.
BASE_FEATURE(kNetworkAnnotationMonitoring,
             "NetworkAnnotationMonitoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This flag is used for enabling New Tab Page triggered prerendering. See
// crbug.com/1462832 for more details of New Tab Page triggered prerendering.
BASE_FEATURE(kNewTabPageTriggerForPrerender2,
             "NewTabPageTriggerForPrerender2",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Don't call the Win32 API PrefetchVirtualMemory when loading chrome.dll inside
// non-browser processes. This is done by passing flags to these processes. This
// prevents pulling the entirety of chrome.dll into physical memory (albeit only
// pri-2 physical memory) under the assumption that during chrome execution,
// portions of the DLL which are used will already be present, hopefully leading
// to less needless memory consumption.
BASE_FEATURE(kNoPreReadMainDll,
             "NoPreReadMainDll",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Adds an "Unsubscribe" action to web push notifications that allows stopping
// notifications from a given origin with a single tap (with an option to undo).
BASE_FEATURE(kNotificationOneTapUnsubscribe,
             "NotificationOneTapUnsubscribe",
             base::FEATURE_DISABLED_BY_DEFAULT);
base::FeatureParam<bool> kNotificationOneTapUnsubscribeUseServiceIntentParam{
    &kNotificationOneTapUnsubscribe, "use_service_intent", false};
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Enables AES keys support in the chrome.enterprise.platformKeys and
// chrome.platformKeys APIs. The new operations include `sign`, `encrypt` and
// `decrypt`. For additional details, see the proposal tracked in b/288880151.
BASE_FEATURE(kPlatformKeysAesEncryption,
             "PlatformKeysAesEncryption",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Disables prerendering on the default search engine predictor. This is useful
// in comparing the impact of the SupportSearchSuggestionForPrerender2 feature
// during its rollout. Once that rollout is complete, this feature should be
// removed and instead we should add a new long-term holdback to
// PreloadingConfig.
BASE_FEATURE(kPrerenderDSEHoldback,
             "PrerenderDSEHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables reading and writing PWA notification permissions from quick settings
// menu.
BASE_FEATURE(kQuickSettingsPWANotifications,
             "QuickSettingsPWA",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
// Keeps accessibility enabled for WebContents as ReadAnything observes changes
// to the active WebContents. This is a holdback study to evaluate the impact of
// the new behavior, whereby the accessibility modes required by ReadyAnything
// are cleared on a WebContents when ReadAnything loses interest in it.
BASE_FEATURE(kReadAnythingPermanentAccessibility,
             "ReadAnythingPermanentAccessibility",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When this feature is enabled, Chrome will register os_update_handler with
// Omaha, to be run on OS upgrade.
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
BASE_FEATURE(kRegisterOsUpdateHandlerWin,
             "RegisterOsUpdateHandlerWin",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

// When this feature is enabled, the network service will restart unsandboxed if
// a previous attempt to launch it sandboxed failed.
BASE_FEATURE(kRestartNetworkServiceUnsandboxedForFailedLaunch,
             "RestartNetworkServiceUnsandboxedForFailedLaunch",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

#if BUILDFLAG(IS_LINUX)
// If true, encrypt new data with the key provided by SecretPortalKeyProvider.
// Otherwise, it will only decrypt existing data.
BASE_FEATURE(kSecretPortalKeyProviderUseForEncryption,
             "SecretPortalKeyProviderUseForEncryption",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

// This flag controls whether to trigger prerendering when the default search
// engine suggests to prerender a search result.
BASE_FEATURE(kSupportSearchSuggestionForPrerender2,
             "SupportSearchSuggestionForPrerender2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Task Manager Desktop Refresh project.
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kTaskManagerDesktopRefresh,
             "TaskManagerDesktopRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

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
// TODO(crbug.com/40198577): Remove this flag once we confirm that blue border
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

#if BUILDFLAG(IS_WIN)
// Disable dynamic code using ACG. Prevents the browser process from generating
// dynamic code or modifying executable code. See comments in
// sandbox/win/src/security_level.h. Only available on Windows 10 RS1 (1607,
// Build 14393) onwards.
BASE_FEATURE(kBrowserDynamicCodeDisabled,
             "BrowserDynamicCodeDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
// This flag controls whether to perform Pak integrity check on startup to
// report statistics for on-disk corruption.
// Disabled on ChromeOS, as dm-verity enforces integrity and the check would
// be redundant.
BASE_FEATURE(kReportPakFileIntegrity,
             "ReportPakFileIntegrity",
#if !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_ANDROID)

// This flag enables the removal of IWAs surface captures from Chrome Tabs
// category in getDisplayMedia() API. When disabled, IWAs surface captures
// show both in Chrome Tabs and Windows.
BASE_FEATURE(kRemovalOfIWAsFromTabCapture,
             "RemovalOfIWAsFromTabCapture",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
