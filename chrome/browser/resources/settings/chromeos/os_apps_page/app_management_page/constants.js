// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The number of apps displayed in app list in the main view before expanding.
 * @const {number}
 */
const NUMBER_OF_APPS_DISPLAYED_DEFAULT = 4;

/**
 * Enumeration of the different subpage types within the app management page.
 * @enum {number}
 * @const
 */
const PageType = {
  MAIN: 0,
  DETAIL: 1,
};

/**
 * A number representation of a Bool. Permission values should be of this type
 * for permissions with value type PermissionValueType.kBool.
 * @enum {number}
 * @const
 */
const Bool = {
  kFalse: 0,
  kTrue: 1,
};

const PwaPermissionType = appManagement.mojom.PwaPermissionType;

const ArcPermissionType = appManagement.mojom.ArcPermissionType;

const AppType = apps.mojom.AppType;

const PermissionValueType = apps.mojom.PermissionValueType;

const TriState = apps.mojom.TriState;

const OptionalBool = apps.mojom.OptionalBool;

const InstallSource = apps.mojom.InstallSource;

// This histogram is also declared and used at chrome/browser/ui/webui/settings/
// chromeos/app_management/app_management_uma.h.
const AppManagementEntryPointsHistogramName = 'AppManagement.EntryPoints';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const AppManagementEntryPoint = {
  AppListContextMenuAppInfoArc: 0,
  AppListContextMenuAppInfoChromeApp: 1,
  AppListContextMenuAppInfoWebApp: 2,
  LauncherContextMenuAppInfoArc: 3,
  LauncherContextMenuAppInfoChromeApp: 4,
  LauncherContextMenuAppInfoWebApp: 5,
  MainViewArc: 6,
  MainViewChromeApp: 7,
  MainViewWebApp: 8,
  OsSettingsMainPage: 9,
};

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const AppManagementUserAction = {
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
};
