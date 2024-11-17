// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_KEEP_ALIVE_PROFILE_KEEP_ALIVE_TYPES_H_
#define CHROME_BROWSER_PROFILES_KEEP_ALIVE_PROFILE_KEEP_ALIVE_TYPES_H_

#include <ostream>

#include "build/build_config.h"

// Refers to what a ScopedProfileKeepAlive's lifetime is tied to, to help
// debugging.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Keep this in sync with ProfileKeepAliveOrigin in enums.xml.
// LINT.IfChange
enum class ProfileKeepAliveOrigin {
  // When a Profile gets created by ProfileManager, it initially has this type
  // of keep-alive. This ensures that the Profile has a refcount >=1, at least
  // until RemoveKeepAlive() gets called.
  //
  // When a kBrowserWindow keep-alive gets added, this one gets removed.
  kWaitingForFirstBrowserWindow = 0,

  // This Profile has browser windows open.
  kBrowserWindow = 1,

  // This Profile is running extensions with persistent background scripts.
  kBackgroundMode = 2,

  // A child off-the-record profile holds a strong reference to its parent.
  kOffTheRecordProfile = 3,

  // This Profile is downloading a file.
  kDownloadInProgress = 4,

  // On macOS, Chrome doesn't exit when all windows are closed. Keep one Profile
  // alive so we can open windows for the last-used Profile when the user
  // "launches" Chrome again.
  //
  // DEPRECATED: Not currently in use, but left here for consistency with
  // enums.xml.
  // kAppControllerMac = 5,

  // In the middle of clearing browsing data, e.g. when the user deletes it via
  // the Profile menu, or during ephemeral profile teardown.
  kClearingBrowsingData = 6,

  // An app (Chrome app, web app, etc.) has a window open.
  kAppWindow = 7,

  // Background sync in progress.
  kBackgroundSync = 8,

  // A notification is active in the system tray.
  kNotification = 9,

  // The user just clicked on a notification. This might cause e.g. a new
  // browser window to open, so wait for the event to finish processing.
  kPendingNotificationClickEvent = 10,

  // There's a visible Push Notification from a Service Worker.
  kInFlightPushMessage = 11,

  // Session restore in progress.
  kSessionRestore = 12,

  // Views wants to keep the BrowserProcess (and Profile) alive, e.g. because
  // a dropdown menu is active.
  kChromeViewsDelegate = 13,

  // A DevTools window is open.
  kDevToolsWindow = 14,

  // A web app permission dialog window is open.
  kWebAppPermissionDialogWindow = 15,

  // Data for Clear on Exit is being deleted.
  kSessionDataDeleter = 16,

  // DEPRECATED: kWebAppProtocolHandlerLaunch = 17,

  // An extension is being updated.
  kExtensionUpdater = 18,

  // This profile is being created (and is used to render GAIA sign-in flow).
  // The profile creation flow either opens a browser window before
  // kProfileCreationFlow keep alive is released or gets aborted without opening
  // a browser window and in that case the profile should be removed.
  kProfileCreationFlow = 19,

  // The user just closed a notification. This might cause writing to the
  // profile's NotificationDatabase, so wait for the event to finish processing.
  kPendingNotificationCloseEvent = 20,

  // The "Send Feedback" WebUI dialog is visible. Because it renders with WebUI,
  // this dialog holds a RenderProcessHost. Closing the Profile before the RPH
  // goes away would cause all sorts of problems...
  kFeedbackDialog = 21,

  // A web app is being updated.
  kWebAppUpdate = 22,

  // Getting web app info for this profile. This is specifically for handling
  // --list-apps switch.
  kGettingWebAppInfo = 23,

  // An extension .crx is being installed.
  kCrxInstaller = 24,

  // The ProfilePickerView dialog is visible. This refers to the System Profile.
  kProfilePickerView = 25,

  // DEPRECATED
  // kCommanderFrontend = 26,

  // UI bubble that may outlive the Browser, especially on Mac.
  kDiceWebSigninInterceptionBubble = 27,

  // Waiting for History menu entries to populate, so we have
  // something to show after the profile is destroyed. macOS-specific.
  kHistoryMenuBridge = 28,

  // DEPRECATED
  // kLacrosMainProfile = 29,

  // This profile is being created, and the SAML flow needs to be completed to
  // finish signin in the user's account.
  kProfileCreationSamlFlow = 30,

  // DEPRECATED
  // kDriveFsNativeMessageHostLacros = 31,

  // Used during the deletion process for the respective profile. Avoids the
  // profile from being randomly unloaded. Useful to keep an ephemeral profile
  // alive until their deletion is completed, after releasing its last keep
  // alive.
  kProfileDeletionProcess = 32,

  // Used when displaying the statistics for a profile in the Profile Picker,
  // when deleting this profile.
  kProfileStatistics = 33,

  // Used during installation of an Isolated Web App.
  kIsolatedWebAppInstall = 34,

  // Used during update of an Isolated Web App.
  kIsolatedWebAppUpdate = 35,

  // A web app is being uninstalled.
  kWebAppUninstall = 36,

  // Used during ForceUnregistration of OsIntegrationManger's sub managers.
  kOsIntegrationForceUnregistration = 37,

  // Used for remote debugging to keep a profile alive when all pages are
  // closed.
  kRemoteDebugging = 38,

  // Used by Headless Command Processor to retain the profile used by the
  // command handler, which does not belong to any window.
  kHeadlessCommand = 39,

  kMaxValue = kHeadlessCommand,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/profile/enums.xml)

std::ostream& operator<<(std::ostream& out,
                         const ProfileKeepAliveOrigin& origin);

#endif  // CHROME_BROWSER_PROFILES_KEEP_ALIVE_PROFILE_KEEP_ALIVE_TYPES_H_
