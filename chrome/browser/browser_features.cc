// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"

#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/net/system_network_context_manager.h"
#endif

namespace features {

#if BUILDFLAG(IS_ANDROID)
// Kill switch for allowing TWAs to autoplay with sound without requiring a user
// gesture to unlock, for parity with PWAs.
BASE_FEATURE(kAllowUnmutedAutoplayForTWA, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// This is used to enable an experiment for modifying confidence cutoff of
// prerender and preconnect for autocomplete action predictor.
BASE_FEATURE(kAutocompleteActionPredictorConfidenceCutoff,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is used to enable an experiment for the bookmarks tree view in the
// side panel, providing users with a hierarchical view of their bookmarks.
BASE_FEATURE(kBookmarksTreeView, base::FEATURE_DISABLED_BY_DEFAULT);

// This is used as a kill switch for Bookmark triggered prerendering. See
// crbug.com/40259793 for more details of Bookmark triggered prerendering.
BASE_FEATURE(kBookmarkTriggerForPrerender2KillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This flag is used for enabling BookmarkBar triggered preconnect.
BASE_FEATURE(kBookmarkTriggerForPreconnect, base::FEATURE_ENABLED_BY_DEFAULT);

// This flag is used for enabling BookmarkBar triggered prefetch.  See
// crbug.com/413259638 for more details of Bookmark triggered prefetching.
BASE_FEATURE(kBookmarkTriggerForPrefetch, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Certificate Transparency on Desktop and Android Browser (CT is
// disabled in Android Webview, see aw_browser_context.cc).
// Enabling CT enforcement requires maintaining a log policy, and the ability to
// update the list of accepted logs. Embedders who are planning to enable this
// should first reach out to chrome-certificate-transparency@google.com.
// On builds where CT is enabled, this flag is also used as an emergency kill
// switch.
BASE_FEATURE(kCertificateTransparencyAskBeforeEnabling,
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
#if !BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kCertVerificationNetworkTime, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kCertVerificationNetworkTime, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Killswitch that guards clearing all user data in the ProfileImpl destructor.
BASE_FEATURE(kClearUserDataUponProfileDestruction,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX)
// Enables usage of os_crypt_async::SecretPortalKeyProvider.  Once
// `kSecretPortalKeyProviderUseForEncryption` is enabled, this flag cannot be
// disabled without losing data.
BASE_FEATURE(kDbusSecretPortal, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

// Destroy profiles when their last browser window is closed, instead of when
// the browser exits.
BASE_FEATURE(kDestroyProfileOnBrowserClose,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// DestroyProfileOnBrowserClose only covers deleting regular (non-System)
// Profiles. This flags lets us destroy the System Profile, as well.
BASE_FEATURE(kDestroySystemProfiles, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing the email of the flex org admin that setup CBCM in the
// management disclosures.
BASE_FEATURE(kFlexOrgManagementDisclosure,
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
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_FEATURE(kInitialExternalExtensions, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
// Adds a "Snooze" action to mute notifications during screen sharing sessions.
BASE_FEATURE(kMuteNotificationSnoozeAction, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// This feature enables monitoring of first-party network requests in order to
// find possible violations. Example: A Chrome policy is set to disabled but the
// network request controlled by that policy is observed.
BASE_FEATURE(kNetworkAnnotationMonitoring, base::FEATURE_ENABLED_BY_DEFAULT);

// This flag is used for enabling New Tab Page triggered prerendering. See
// crbug.com/1462832 for more details of New Tab Page triggered prerendering.
BASE_FEATURE(kNewTabPageTriggerForPrerender2, base::FEATURE_ENABLED_BY_DEFAULT);

// This flag is used for enabling New Tab Page triggered prefetch. See
// crbug.com/421941586 for more details of New Tab Page triggered prefetching.
BASE_FEATURE(kNewTabPageTriggerForPrefetch, base::FEATURE_DISABLED_BY_DEFAULT);

// Adds an "Unsubscribe" action to web push notifications that allows stopping
// notifications from a given origin with a single tap (with an option to undo).
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kNotificationOneTapUnsubscribeOnDesktop,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// When this feature is enabled, Chrome will register os_update_handler with
// Omaha, to be run on OS upgrade.
BASE_FEATURE(kRegisterOsUpdateHandlerWin, base::FEATURE_ENABLED_BY_DEFAULT);
// When this feature is enabled, Chrome will install the
// platform_experience_helper.
BASE_FEATURE(kInstallPlatformExperienceHelperWin,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

// When this feature is enabled, the network service will restart unsandboxed if
// a previous attempt to launch it sandboxed failed.
BASE_FEATURE(kRestartNetworkServiceUnsandboxedForFailedLaunch,
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
BASE_FEATURE(kSandboxExternalProtocolBlocked, base::FEATURE_ENABLED_BY_DEFAULT);
// Enabled in M100. Flag to be removed in M106
BASE_FEATURE(kSandboxExternalProtocolBlockedWarning,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX)
// If true, encrypt new data with the key provided by SecretPortalKeyProvider.
// Otherwise, it will only decrypt existing data.
BASE_FEATURE(kSecretPortalKeyProviderUseForEncryption,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

// Enables migration of the network context data from `unsandboxed_data_path` to
// `data_path`. See the explanation in network_context.mojom.
BASE_FEATURE(kTriggerNetworkDataMigration,
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
//  The blue border behavior used to (still does, see below) cause problems on
//  ChromeOS - see crbug.com/1320262 (fixed). This flag is introduced as means
//  of disabling this feature in case of possible future regressions.
//
// TODO(crbug.com/40198577): Remove this flag once we confirm that blue border
// works fine on ChromeOS.
//
// b/279051234: We suspect the tab sharing blue border may cause a bad issue
// on ChromeOS where a window can not be interacted at all. Disable the feature
// on ChromeOS.
BASE_FEATURE(kTabCaptureBlueBorderCrOS, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables runtime detection of USB devices which provide a WebUSB landing page
// descriptor.
BASE_FEATURE(kWebUsbDeviceDetection, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Disable dynamic code using ACG. Prevents the browser process from generating
// dynamic code or modifying executable code. See comments in
// sandbox/win/src/security_level.h. Only available on Windows 10 RS1 (1607,
// Build 14393) onwards.
BASE_FEATURE(kBrowserDynamicCodeDisabled, base::FEATURE_DISABLED_BY_DEFAULT);

// The Chrome DLL can be pre-read with ::PrefetchVirtualMemory() from the
// browser or a child process. Pre-reading is supposed to bring the whole DLL in
// physical memory more efficiently than a series of hard faults. However,
// pre-reading consumes a non-trivial amount of CPU even when the DLL is already
// in physical memory and it may not be necessary to have the full DLL in
// physical memory (space taken by unused parts of the DLL could potentially be
// used for more important stuff). This file has multiple features to experiment
// with policies for pre-reading the Chrome DLL in child processes. The
// `kPrefetchVirtualMemoryPolicy` feature defined elsewhere controls pre-reading
// the Chrome DLL from the browser process.

// When enabled, child processes never pre-read the Chrome DLL.
BASE_FEATURE(kNoPreReadMainDll, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, child processes don't pre-read the Chrome DLL if we believe the
// Chrome DLL is on an SSD (i.e. pre-read only on spinning disk).
BASE_FEATURE(kNoPreReadMainDllIfSsd, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the browser process suppresses pre-read in child processes
// shortly after browser startup, where "shortly after" is dictated by the
// feature param below. This is thought to be a productive strategy since the
// browser process will have recently pre-read the DLL during browser
// startup. In that case, the browser process has recently pre-read the DLL so
// pre-reading again is thought to be counter-productive (CPU consumption for no
// gains).
BASE_FEATURE(kNoPreReadMainDllStartup, base::FEATURE_DISABLED_BY_DEFAULT);

// Time after browser startup during which child processes don't pre-read the
// Chrome DLL when `kNoPreReadMainDllStartup` is enabled.
const base::FeatureParam<base::TimeDelta>
    kNoPreReadMainDllStartup_StartupDuration{&kNoPreReadMainDllStartup,
                                             "no-preread-dll-startup-time",
                                             base::Minutes(2)};

// When enabled, the browser process will re-launch itself when launched with
// an elevated linked token. The re-launched browser will use the token from
// the Windows Shell (explorer.exe), which is typically non-elevated.
BASE_FEATURE(kAutoDeElevate, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
// This flag controls whether to perform Pak integrity check on startup to
// report statistics for on-disk corruption.
// Disabled on ChromeOS, as dm-verity enforces integrity and the check would
// be redundant.
BASE_FEATURE(kReportPakFileIntegrity,
#if !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_ANDROID)

// This flag enables the removal of IWAs surface captures from Chrome Tabs
// category in getDisplayMedia() API. When disabled, IWAs surface captures
// show both in Chrome Tabs and Windows.
BASE_FEATURE(kRemovalOfIWAsFromTabCapture, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
