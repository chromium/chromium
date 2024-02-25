// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {Action} from 'chrome://resources/js/store.js';

export interface AddAppAction extends Action {
  name: 'add-app';
  app: App;
}

export interface ChangeAppAction extends Action {
  name: 'change-app';
  app: App;
}

export interface RemoveAppAction extends Action {
  name: 'remove-app';
  id: string;
}

export interface UpdateSelectedAppIdAction extends Action {
  name: 'update-selected-app-id';
  value: string|null;
}

export interface UpdateSubAppToParentAppIdAction extends Action {
  name: 'update-sub-app-to-parent-app-id';
  subApp: string;
  parent: string|null;
}

export type AppManagementActions = AddAppAction|ChangeAppAction|RemoveAppAction|
    UpdateSelectedAppIdAction|UpdateSubAppToParentAppIdAction;

export function addApp(app: App): AddAppAction {
  return {
    name: 'add-app',
    app,
  };
}

export function changeApp(app: App): ChangeAppAction {
  return {
    name: 'change-app',
    app,
  };
}

export function removeApp(id: string): RemoveAppAction {
  return {
    name: 'remove-app',
    id,
  };
}

export function updateSelectedAppId(appId: string|
                                    null): UpdateSelectedAppIdAction {
  return {
    name: 'update-selected-app-id',
    value: appId,
  };
}

export function updateSubAppToParentAppId(
    appId: string, parentAppId: string|null): UpdateSubAppToParentAppIdAction {
  return {
    name: 'update-sub-app-to-parent-app-id',
    subApp: appId,
    parent: parentAppId,
  };
}
