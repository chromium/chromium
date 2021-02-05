// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

// #import 'chrome://resources/mojo/skia/public/mojom/image_info.mojom-lite.js';
// #import 'chrome://resources/mojo/skia/public/mojom/bitmap.mojom-lite.js';
// #import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// #import '/app-management/file_path.mojom-lite.js';
// #import '/app-management/image.mojom-lite.js';
// #import '/app-management/types.mojom-lite.js';
// #import '/app-management/app_management.mojom-lite.js';
// clang-format on

/**
 * The number of apps displayed in app list in the main view before expanding.
 * @const {number}
 */
/* #export */ const NUMBER_OF_APPS_DISPLAYED_DEFAULT = 4;

/**
 * Enumeration of the different subpage types within the app management page.
 * @enum {number}
 * @const
 */
/* #export */ const PageType = {
  MAIN: 0,
  DETAIL: 1,
};

/**
 * A number representation of a Bool. Permission values should be of this type
 * for permissions with value type PermissionValueType.kBool.
 * @enum {number}
 * @const
 */
/* #export */ const Bool = {
  kFalse: 0,
  kTrue: 1,
};

/* #export */ const PwaPermissionType = appManagement.mojom.PwaPermissionType;

/* #export */ const PluginVmPermissionType =
    appManagement.mojom.PluginVmPermissionType;

/* #export */ const ArcPermissionType = appManagement.mojom.ArcPermissionType;

/* #export */ const AppType = apps.mojom.AppType;

/* #export */ const PermissionValueType = apps.mojom.PermissionValueType;

/* #export */ const TriState = apps.mojom.TriState;

/* #export */ const OptionalBool = apps.mojom.OptionalBool;

/* #export */ const InstallSource = apps.mojom.InstallSource;

// This histogram is also declared and used at chrome/browser/ui/webui/settings/
// chromeos/app_management/app_management_uma.h.
/* #export */ const AppManagementEntryPointsHistogramName =
    'AppManagement.EntryPoints';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
/* #export */ const AppManagementEntryPoint = {
  AppListContextMenuAppInfoArc: 0,
  AppListContextMenuAppInfoChromeApp: 1,
  AppListContextMenuAppInfoWebApp: 2,
  ShelfContextMenuAppInfoArc: 3,
  ShelfContextMenuAppInfoChromeApp: 4,
  ShelfContextMenuAppInfoWebApp: 5,
  MainViewArc: 6,
  MainViewChromeApp: 7,
  MainViewWebApp: 8,
  OsSettingsMainPage: 9,
  MainViewPluginVm: 10,
  DBusServicePluginVm: 11,
};

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
/* #export */ const AppManagementUserAction = {
  ViewOpened: 0,
  NativeSettingsOpened: 1,
  UninstallDialogLaunched: 2,
  PinToShelfTurnedOn: 3,
  PinToShelfTurnedOff: 4,
  NotificationsTurnedOn: 5,
  NotificationsTurnedOff: 6,
  LocationTurnedOn: 7,
  LocationTurnedOff: 8,
  CameraTurnedOn: 9,
  CameraTurnedOff: 10,
  MicrophoneTurnedOn: 11,
  MicrophoneTurnedOff: 12,
  ContactsTurnedOn: 13,
  ContactsTurnedOff: 14,
  StorageTurnedOn: 15,
  StorageTurnedOff: 16,
  PrintingTurnedOn: 17,
  PrintingTurnedOff: 18,
};
