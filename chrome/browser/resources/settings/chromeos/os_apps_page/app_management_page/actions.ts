// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

import {Action} from 'chrome://resources/ash/common/store/store.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

export type AddAppAction = Action&{
  name: 'add-app',
  app: App,
};

export type ChangeAppAction = Action&{
  name: 'change-app',
  app: App,
};

export type RemoveAppAction = Action&{
  name: 'remove-app',
  id: string,
};

export type UpdateSelectedAppIdAction = Action&{
  name: 'update-selected-app-id',
  value: string | null,
};

export type AppManagementActions =
    AddAppAction|ChangeAppAction|RemoveAppAction|UpdateSelectedAppIdAction;

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
