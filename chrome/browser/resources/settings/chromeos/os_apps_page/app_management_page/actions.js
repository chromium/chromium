// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/ash/common/store/store.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

/**
 * @param {App} app
 * @return {Action}
 */
export function addApp(app) {
  return {
    name: 'add-app',
    app: app,
  };
}

/**
 * @param {App} app
 * @return {Action}
 */
export function changeApp(app) {
  return {
    name: 'change-app',
    app: app,
  };
}

/**
 * @param {string} id
 * @return {Action}
 */
export function removeApp(id) {
  return {
    name: 'remove-app',
    id: id,
  };
}

/**
 * @param {?string} appId
 * @return {Action}
 */
export function updateSelectedAppId(appId) {
  return {
    name: 'update-selected-app-id',
    value: appId,
  };
}
