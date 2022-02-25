// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for App Management.
 */

/**
 * @typedef {appManagement.mojom.App}
 */
let App;

/**
 * @typedef {appManagement.mojom.ExtensionAppPermissionMessage}
 */
let ExtensionAppPermissionMessage;

/**
 * @typedef {apps.mojom.Permission}
 */
let Permission;

/**
 * Maps app ids to Apps.
 * @typedef {!Object<string, App>}
 */
let AppMap;

/**
 * @typedef {{
 *   apps: !AppMap,
 *   selectedAppId: ?string,
 * }}
 */
let AppManagementPageState;

/**
 * @typedef {apps.mojom.WindowMode}
 */
let WindowMode;

/**
 * Must be kept in sync with
 * ui/webui/resources/cr_components/app_management/constants.ts
 * @enum {number}
 */
const AppManagementEntryPointType = {
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
  MainViewBorealis: 12,
};

/**
 * Must be kept in sync with
 * ui/webui/resources/cr_components/app_management/constants.ts
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
  PrintingTurnedOn: 17,
  PrintingTurnedOff: 18,
  ResizeLockTurnedOn: 19,
  ResizeLockTurnedOff: 20,
  PreferredAppTurnedOn: 21,
  PreferredAppTurnedOff: 22,
  SupportedLinksListShown: 23,
  OverlappingAppsDialogShown: 24,
};

/**
 * @constructor
 * @extends {HTMLElement}
 */
function AppManamentPermissionItemElement() {}

/** @type {boolean} */
AppManamentPermissionItemElement.prototype.permissionType;

AppManamentPermissionItemElement.prototype.syncPermission = function() {};
AppManamentPermissionItemElement.prototype.resetToggle = function() {};

/**
 * @constructor
 * @extends {HTMLElement}
 */
function AppManagementToggleRowElement() {}

/** @return {boolean} */
AppManagementToggleRowElement.prototype.isChecked = function() {};
