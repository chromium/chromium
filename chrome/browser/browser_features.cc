// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if defined(OS_WIN)
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
const base::Feature kDestroyProfileOnBrowserClose{
    "DestroyProfileOnBrowserClose", base::FEATURE_DISABLED_BY_DEFAULT};

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

#if defined(OS_MAC)
// Enables integration with the macOS feature Universal Links.
const base::Feature kEnableUniveralLinks{"EnableUniveralLinks",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables reading and writing PWA notification permissions from quick settings
// menu.
const base::Feature kQuickSettingsPWANotifications{
    "QuickSettingsPWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables being able to zoom a web page by double tapping in Chrome OS tablet
// mode.
const base::Feature kDoubleTapToZoomInTabletMode{
    "DoubleTapToZoomInTabletMode", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Adds an item to the context menu that copies a link to the page with the
// selected text highlighted.
const base::Feature kCopyLinkToText{"CopyLinkToText",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Adds a "Snooze" action to mute notifications during screen sharing sessions.
const base::Feature kMuteNotificationSnoozeAction{
    "MuteNotificationSnoozeAction", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
// Results in remembering fonts used at the time of fcp, and prewarming those
// fonts on subsequent loading of search results pages for the default search
// engine.
const base::Feature kPrewarmSearchResultsPageFonts{
    "PrewarmSearchResultsPageFonts", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Shows a confirmation dialog when updates to PWAs identity (name and icon)
// have been detected.
const base::Feature kPwaUpdateDialogForNameAndIcon{
  "PwaUpdateDialogForNameAndIcon",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Gates sandboxed iframe navigation toward external protocol behind any of:
// - allow-popups
// - allow-top-navigation
// - allow-top-navigation-with-user-gesture (+ user gesture)
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
const base::Feature kSandboxExternalProtocolBlocked{
    "SandboxExternalProtocolBlocked", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a blue-border is drawn around shared tabs.
// If disabled, the blue border is *never* used, no matter what any other
// flag might say.
// If enabled, the blue border is *generally* used, but other flags might
// still disable it for specific cases.
const base::Feature kTabCaptureBlueBorder{"TabCaptureBlueBorder",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// This flag is subordinate to |kTabCaptureBlueBorder|:
// * If |kTabCaptureBlueBorder| is disabled, the blue border is always disabled,
//    and this flag has no effect.
// * If |kTabCaptureBlueBorder| and
//   |kTabCaptureBlueBorderForSelfCaptureRegionCaptureOT| are both enabled,
//   the blue-border is always drawn.
// * If |kTabCaptureBlueBorder| is enabled but
//   |kTabCaptureBlueBorderForSelfCaptureRegionCaptureOT| is disabled,
//   then the blue-border tab-capture-indicator will NOT be drawn if the
//   following conditions apply:
//   1. A single capture of the tab exists, and it is self-capture (a document
//      is tab-capturing the very tab in which the document is loaded).
//   2. The capturing document is opted-into Region Capture. (Either through an
//      origin trial or through enabling Experimental Web Platforms features.)
const base::Feature kTabCaptureBlueBorderForSelfCaptureRegionCaptureOT{
    "TabCaptureBlueBorderForSelfCaptureRegionCaptureOT",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables migration of the network context data from `unsandboxed_data_path` to
// `data_path`. See the explanation in network_context.mojom.
const base::Feature kTriggerNetworkDataMigration {
  "TriggerNetworkDataMigration",
#if defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables runtime detection of USB devices which provide a WebUSB landing page
// descriptor.
const base::Feature kWebUsbDeviceDetection{"WebUsbDeviceDetection",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Enables Certificate Transparency on Android.
const base::Feature kCertificateTransparencyAndroid{
    "CertificateTransparencyAndroid", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kLargeFaviconFromGoogle{"LargeFaviconFromGoogle",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kLargeFaviconFromGoogleSizeInDip{
    &kLargeFaviconFromGoogle, "favicon_size_in_dip", 128};

}  // namespace features
