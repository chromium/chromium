// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createInitialState} from 'chrome://resources/cr_components/app_management/util.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {Action} from 'chrome://resources/js/cr/ui/store.js';

import {addApp, changeApp, removeApp} from './actions.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementStore} from './store.js';

let initialized = false;

async function init() {
  assert(!initialized);

  const {apps: initialApps} =
      await BrowserProxy.getInstance().handler.getApps();
  const initialState = createInitialState(initialApps);
  AppManagementStore.getInstance().init(initialState);

  const callbackRouter = BrowserProxy.getInstance().callbackRouter;

  callbackRouter.onAppAdded.addListener(onAppAdded);
  callbackRouter.onAppChanged.addListener(onAppChanged);
  callbackRouter.onAppRemoved.addListener(onAppRemoved);

  initialized = true;
}

/**
 * @param {Action} action
 */
function dispatch(action) {
  AppManagementStore.getInstance().dispatch(action);
}

/**
 * @param {App} app
 */
function onAppAdded(app) {
  dispatch(addApp(app));
}

/**
 * @param {App} app
 */
function onAppChanged(app) {
  dispatch(changeApp(app));
}

/**
 * @param {string} appId
 */
function onAppRemoved(appId) {
  dispatch(removeApp(appId));
}

init();
