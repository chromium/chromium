// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module of functions which produce a new page state in response
 * to an action. Reducers (in the same sense as Array.prototype.reduce) must be
 * pure functions: they must not modify existing state objects, or make any API
 * calls.
 */

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';

import {AddAppAction, AppManagementActions, ChangeAppAction, RemoveAppAction} from './actions.js';
import {AppManagementPageState, AppMap} from './store.js';

function addApp(apps: AppMap, action: AddAppAction): AppMap {
  if (apps[action.app.id]) {
    const stringifyApp = (app: App): string => {
      return `id: ${app.id}, type: ${app.type}, install source: ${
          app.installReason} title: ${app.title}`;
    };
    const errorMessage = `Attempted to add an app that already exists.
                          New app: ${stringifyApp(action.app)}.
                          Old app: ${stringifyApp(apps[action.app.id])}.`;
    assertNotReached(errorMessage);
  }
  return {...apps, [action.app.id]: action.app};
}

function changeApp(apps: AppMap, action: ChangeAppAction): AppMap {
  // If the app doesn't exist, that means that the app that has been changed
  // does not need to be shown in App Management.
  if (!apps[action.app.id]) {
    return apps;
  }
  return {...apps, [action.app.id]: action.app};
}

function removeApp(apps: AppMap, action: RemoveAppAction): AppMap {
  if (!apps.hasOwnProperty(action.id)) {
    return apps;
  }

  delete apps[action.id];
  return {...apps};
}

export function updateApps(apps: AppMap, action: AppManagementActions): AppMap {
  switch (action.name) {
    case 'add-app':
      return addApp(apps, action as AddAppAction);
    case 'change-app':
      return changeApp(apps, action as ChangeAppAction);
    case 'remove-app':
      return removeApp(apps, action as RemoveAppAction);
    default:
      return apps;
  }
}

function updateSelectedAppId(
    selectedAppId: string|null, action: AppManagementActions): string|null {
  switch (action.name) {
    case 'update-selected-app-id':
      return action.value;
    case 'remove-app':
      if (selectedAppId === action.id) {
        return null;
      }
      return selectedAppId;
    default:
      return selectedAppId;
  }
}

function updateSubAppToParentAppId(
    subAppToParentAppId: Record<string, string>,
    action: AppManagementActions): Record<string, string> {
  switch (action.name) {
    case 'update-sub-app-to-parent-app-id':
      if (action.parent) {
        return {...subAppToParentAppId, [action.subApp]: action.parent};
      }
      delete subAppToParentAppId[action.subApp];
      return {...subAppToParentAppId};
    default:
      return subAppToParentAppId;
  }
}

/**
 * Root reducer for the App Management page. This is called by the store in
 * response to an action, and the return value is used to update the UI.
 */
export function reduceAction(
    state: AppManagementPageState,
    action: AppManagementActions): AppManagementPageState {
  return {
    apps: updateApps(state.apps, action),
    selectedAppId: updateSelectedAppId(state.selectedAppId, action),
    subAppToParentAppId:
        updateSubAppToParentAppId(state.subAppToParentAppId, action),
  };
}
