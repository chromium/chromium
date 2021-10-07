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

// Destroy profiles when their last browser window is closed, instead of when
// the browser exits.
const base::Feature kDestroyProfileOnBrowserClose{
    "DestroyProfileOnBrowserClose", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables executing the browser commands sent by the NTP promos.
const base::Feature kPromoBrowserCommands{"PromoBrowserCommands",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Parameter name for the promo browser command ID provided along with
// kPromoBrowserCommands.
// The value of this parameter should be parsable as an unsigned integer and
// should map to one of the browser commands specified in:
// ui/webui/resources/js/browser_command/browser_command.mojom
const char kBrowserCommandIdParam[] = "BrowserCommandIdParam";

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

// Shows a confirmation dialog when updates to PWAs identity (name and icon)
// have been detected.
const base::Feature kPwaUpdateDialogForNameAndIcon{
    "PwaUpdateDialogForNameAndIcon", base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
// Enables taking snapshots of the user data directory after a major
// milestone update and restoring them after a version rollback.
const base::Feature kUserDataSnapshot{"UserDataSnapshot",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

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

// Enables migration of the network context data from `unsandboxed_data_path` to
// `data_path`. See the explanation in network_context.mojom.
const base::Feature kTriggerNetworkDataMigration{
    "TriggerNetworkDataMigration", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldTriggerNetworkDataMigration() {
#if defined(OS_WIN)
  // On Windows, if sandbox enabled means data must be migrated.
  if (SystemNetworkContextManager::IsNetworkSandboxEnabled())
    return true;
#endif  // defined(OS_WIN)
  if (base::FeatureList::IsEnabled(kTriggerNetworkDataMigration))
    return true;
  return false;
}

// Enables runtime detection of USB devices which provide a WebUSB landing page
// descriptor.
const base::Feature kWebUsbDeviceDetection{"WebUsbDeviceDetection",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
