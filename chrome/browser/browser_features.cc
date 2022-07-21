// Copyright 2018 The Chromium Authors. All rights reserved.
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
const base::Feature kClosedTabCache{"ClosedTabCache",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Whether or not to delegate color queries from the ThemeProvider to the
// ColorProvider.
const base::Feature kColorProviderRedirectionForThemeProvider = {
    "ColorProviderRedirectionForThemeProvider",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Destroy profiles when their last browser window is closed, instead of when
// the browser exits.
const base::Feature kDestroyProfileOnBrowserClose {
  "DestroyProfileOnBrowserClose",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
};
#else
      base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

// DestroyProfileOnBrowserClose only covers deleting regular (non-System)
// Profiles. This flags lets us destroy the System Profile, as well.
const base::Feature kDestroySystemProfiles{"DestroySystemProfiles",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Nukes profile directory before creating a new profile using
// ProfileManager::CreateMultiProfileAsync().
const base::Feature kNukeProfileBeforeCreateMultiAsync{
    "NukeProfileBeforeCreateMultiAsync", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables executing the browser commands sent by the NTP promos.
const base::Feature kPromoBrowserCommands{"PromoBrowserCommands",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Parameter name for the promo browser command ID provided along with
// kPromoBrowserCommands.
// The value of this parameter should be parsable as an unsigned integer and
// should map to one of the browser commands specified in:
// ui/webui/resources/js/browser_command/browser_command.mojom
const char kBrowserCommandIdParam[] = "BrowserCommandIdParam";

// Enables using policy::ManagementService to get the browser's and platform
// management state everywhere.
const base::Feature kUseManagementService{"UseManagementService",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_MAC)
// Enables integration with the macOS feature Universal Links.
const base::Feature kEnableUniveralLinks{"EnableUniveralLinks",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables reading and writing PWA notification permissions from quick settings
// menu.
const base::Feature kQuickSettingsPWANotifications{
    "QuickSettingsPWA", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Enables being able to zoom a web page by double tapping in Chrome OS tablet
// mode.
const base::Feature kDoubleTapToZoomInTabletMode{
    "DoubleTapToZoomInTabletMode", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !BUILDFLAG(IS_ANDROID)
// Adds an item to the context menu that copies a link to the page with the
// selected text highlighted.
const base::Feature kCopyLinkToText{"CopyLinkToText",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Adds a "Snooze" action to mute notifications during screen sharing sessions.
const base::Feature kMuteNotificationSnoozeAction{
    "MuteNotificationSnoozeAction", base::FEATURE_DISABLED_BY_DEFAULT};
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
const base::Feature kSandboxExternalProtocolBlocked{
    "SandboxExternalProtocolBlocked", base::FEATURE_ENABLED_BY_DEFAULT};
// Enabled in M100. Flag to be removed in M106
const base::Feature kSandboxExternalProtocolBlockedWarning{
    "SandboxExternalProtocolBlockedWarning", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables migration of the network context data from `unsandboxed_data_path` to
// `data_path`. See the explanation in network_context.mojom.
const base::Feature kTriggerNetworkDataMigration {
  "TriggerNetworkDataMigration",
#if BUILDFLAG(IS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables runtime detection of USB devices which provide a WebUSB landing page
// descriptor.
const base::Feature kWebUsbDeviceDetection{"WebUsbDeviceDetection",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
// Enables Certificate Transparency on Android.
const base::Feature kCertificateTransparencyAndroid{
    "CertificateTransparencyAndroid", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

const base::Feature kLargeFaviconFromGoogle{"LargeFaviconFromGoogle",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kLargeFaviconFromGoogleSizeInDip{
    &kLargeFaviconFromGoogle, "favicon_size_in_dip", 128};

// Enables the use of a `ProfileManagerObserver` to trigger the post profile
// init step of the browser startup. This affects the initialization order of
// some features with the goal to improve startup performance in some cases.
// See https://bit.ly/chromium-startup-no-guest-profile.
const base::Feature kObserverBasedPostProfileInit{
    "ObserverBasedPostProfileInit", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the static key pinning list can be updated via component
// updater.
#if BUILDFLAG(IS_ANDROID)
const base::Feature kKeyPinningComponentUpdater{
    "KeyPinningComponentUpdater", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kKeyPinningComponentUpdater{
    "KeyPinningComponentUpdater", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// When this feature is enabled, the network service will restart unsandboxed if
// a previous attempt to launch it sandboxed failed.
const base::Feature kRestartNetworkServiceUnsandboxedForFailedLaunch{
    "RestartNetworkServiceUnsandboxedForFailedLaunch",
    base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_WIN)
// When this feature is enabled, metrics are gathered regarding the performance
// and reliability of app-bound encryption primitives on a background thread.
const base::Feature kAppBoundEncryptionMetrics{
    "AppBoundEncryptionMetrics", base::FEATURE_ENABLED_BY_DEFAULT};
#endif
}  // namespace features
