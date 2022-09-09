// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

/**
 * @param {App} app
 */
export function addApp(app) {
  return {
    name: 'add-app',
    app: app,
  };
}

/**
 * @param {App} app
 */
export function changeApp(app) {
  return {
    name: 'change-app',
    app: app,
  };
}

/**
 * @param {string} id
 */
export function removeApp(id) {
  return {
    name: 'remove-app',
    id: id,
  };
}

/**
 * @param {?string} appId
 */
export function updateSelectedAppId(appId) {
  return {
    name: 'update-selected-app-id',
    value: appId,
  };
}
