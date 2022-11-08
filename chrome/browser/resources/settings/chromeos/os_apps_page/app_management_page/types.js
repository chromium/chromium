// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for App Management.
 */

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

/**
 * Maps app ids to Apps.
 * @typedef {!Object<string, App>}
 */
export let AppMap;

/**
 * @typedef {{
 *   apps: !AppMap,
 *   selectedAppId: ?string,
 * }}
 */
export let AppManagementPageState;

/**
 * Must be kept in sync with
 * ui/webui/resources/cr_components/app_management/constants.ts
 * @enum {number}
 */
export const AppManagementEntryPointType = {
  APP_LIST_CONTEXT_MENU_APP_INFO_ARC: 0,
  APP_LIST_CONTEXT_MENU_APP_INFO_CHROME_APP: 1,
  APP_LIST_CONTEXT_MENU_APP_INFO_WEB_APP: 2,
  SHELF_CONTEXT_MENU_APP_INFO_ARC: 3,
  SHELF_CONTEXT_MENU_APP_INFO_CHROME_APP: 4,
  SHELF_CONTEXT_MENU_APP_INFO_WEB_APP: 5,
  MAIN_VIEW_ARC: 6,
  MAIN_VIEW_CHROME_APP: 7,
  MAIN_VIEW_WEB_APP: 8,
  OS_SETTINGS_MAIN_PAGE: 9,
  MAIN_VIEW_PLUGIN_VM: 10,
  D_BUS_SERVICE_PLUGIN_VM: 11,
  MAIN_VIEW_BOREALIS: 12,
};

/**
 * Must be kept in sync with
 * ui/webui/resources/cr_components/app_management/constants.ts
 * @enum {number}
 */
export const AppManagementUserAction = {
  VIEW_OPENED: 0,
  NATIVE_SETTINGS_OPENED: 1,
  UNINSTALL_DIALOG_LAUNCHED: 2,
  PIN_TO_SHELF_TURNED_ON: 3,
  PIN_TO_SHELF_TURNED_OFF: 4,
  NOTIFICATIONS_TURNED_ON: 5,
  NOTIFICATIONS_TURNED_OFF: 6,
  LOCATION_TURNED_ON: 7,
  LOCATION_TURNED_OFF: 8,
  CAMERA_TURNED_ON: 9,
  CAMERA_TURNED_OFF: 10,
  MICROPHONE_TURNED_ON: 11,
  MICROPHONE_TURNED_OFF: 12,
  CONTACTS_TURNED_ON: 13,
  CONTACTS_TURNED_OFF: 14,
  STORAGE_TURNED_ON: 15,
  STORAGE_TURNED_OFF: 16,
  PRINTING_TURNED_ON: 17,
  PRINTING_TURNED_OFF: 18,
  RESIZE_LOCK_TURNED_ON: 19,
  RESIZE_LOCK_TURNED_OFF: 20,
  PREFERRED_APP_TURNED_ON: 21,
  PREFERRED_APP_TURNED_OFF: 22,
  SUPPORTED_LINKS_LIST_SHOWN: 23,
  OVERLAPPING_APPS_DIALOG_SHOWN: 24,
};

/**
 * TODO(crbug/1315757)
 * When dependent code is converted to TS, this type can be imported from
 * chrome://resources/cr_components/app_management/permission_item.js
 *
 * @constructor
 * @extends {HTMLElement}
 */
export function AppManagementPermissionItemElement() {}

/** @type {boolean} */
AppManagementPermissionItemElement.prototype.permissionType;

AppManagementPermissionItemElement.prototype.syncPermission = function() {};
AppManagementPermissionItemElement.prototype.resetToggle = function() {};

/**
 * @constructor
 * @extends {HTMLElement}
 */
export function AppManagementToggleRowElement() {}

/** @return {boolean} */
AppManagementToggleRowElement.prototype.isChecked = function() {};
