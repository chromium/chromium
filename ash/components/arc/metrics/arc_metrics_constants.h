// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_CONSTANTS_H_
#define ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_CONSTANTS_H_

namespace arc {

// Native bridge types for UMA recording (Arc.NativeBridge). These values are
// persisted to logs, and should therefore never be renumbered nor reused.
// Should be synced with ArcNativeBridgeType in
// tools/metrics/histograms/enums.xml.
enum class NativeBridgeType {
  // Native bridge value has not been received from the container yet.
  UNKNOWN = 0,
  // Native bridge is not used.
  NONE = 1,
  // Using houdini translator.
  HOUDINI = 2,
  // Using ndk-translation translator.
  NDK_TRANSLATION = 3,
  kMaxValue = NDK_TRANSLATION,
};

// Defines ARC App user interaction types to track how users use ARC apps.
// These enums are used to define the buckets for an enumerated UMA histogram
// and need to be synced with the ArcUserInteraction enum in
// tools/metrics/histograms/metadata/arc/enums.xml.
enum class UserInteractionType {
  // Default to not user-initiated.
  // Can be used temporarily for a new action path or to denote an action
  // that was not directly user-initiated.
  NOT_USER_INITIATED = 0,

  // User started an app from the launcher.
  APP_STARTED_FROM_LAUNCHER = 1,

  // User started an app from a context menu click in the launcher.
  APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU = 2,

  // User started an app from a launcher search result.
  APP_STARTED_FROM_LAUNCHER_SEARCH = 3,

  // User started an app from a a context menu click on a search result.
  APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU = 4,

  // User started a suggested app in the launcher.
  APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP = 5,

  // User started a suggested app using the context menu in the launcher.
  APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP_CONTEXT_MENU = 6,

  // User started an app from the shelf.
  APP_STARTED_FROM_SHELF = 7,

  // User started an app from the shelf using the context menu.
  // TODO(crbug.com/862901): Record this separately from APP_STARTED_FROM_SHELF
  APP_STARTED_FROM_SHELF_CONTEXT_MENU = 8,

  // User started an app from settings.
  APP_STARTED_FROM_SETTINGS = 9,

  // User interacted with an ARC++ notification. Dismissal of notifications such
  // as closing and swiping out are not being considered.
  NOTIFICATION_INTERACTION = 10,

  // User interacted with the content window.
  APP_CONTENT_WINDOW_INTERACTION = 11,

  // User started an app from chrome.arcAppsPrivate.launchApp.
  APP_STARTED_FROM_EXTENSION_API = 12,

  // User started note-taking app from stylus tools.
  APP_STARTED_FROM_STYLUS_TOOLS = 13,

  // User started an app by opening files in the file manager.
  APP_STARTED_FROM_FILE_MANAGER = 14,

  // User started an app by left-clicking on links in the browser.
  APP_STARTED_FROM_LINK = 15,

  // User started an app from context menu by right-clicking on links in the
  // browser.
  APP_STARTED_FROM_LINK_CONTEXT_MENU = 16,

  // User started an app from Smart Text Selection context menu.
  APP_STARTED_FROM_SMART_TEXT_SELECTION_CONTEXT_MENU = 17,

  // User started an app from the Kiosk Next Home app.
  APP_STARTED_FROM_KIOSK_NEXT_HOME = 18,

  // User interacted with an app using a gamepad.
  GAMEPAD_INTERACTION = 19,

  // User started an app from entering URL in the Omnibox in the browser.
  APP_STARTED_FROM_OMNIBOX = 20,

  // User started an app from Chrome OS sharesheet.
  APP_STARTED_FROM_SHARESHEET = 21,

  // User started an app from Chrome OS full restore.
  APP_STARTED_FROM_FULL_RESTORE = 22,

  // User started an app from another app.
  APP_STARTED_FROM_OTHER_APP = 23,

  // User started the app by interacting with the App Install Service installer.
  APP_STARTED_FROM_INSTALLER = 24,

  kMaxValue = APP_STARTED_FROM_INSTALLER,
};

// Enumerates relevant Mojo connections.
// These values are  persisted to logs, and should therefore never be renumbered
// nor reused. Should be synced with ArcMojoConnectionType in
// tools/metrics/histograms/enums.xml.
enum class MojoConnectionType {
  // Mojo connection to AppLauncher was lost.
  APP_LAUNCHER = 0,

  // Mojo connection to IntentHelper was lost.
  INTENT_HELPER = 1,

  kMaxValue = INTENT_HELPER,
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_CONSTANTS_H_
