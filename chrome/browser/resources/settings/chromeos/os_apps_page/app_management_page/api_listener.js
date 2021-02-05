// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {Action} from 'chrome://resources/js/cr/ui/store.m.js';
// #import {BrowserProxy} from './browser_proxy.m.js';
// #import {createInitialState} from './util.m.js';
// #import {AppManagementStore} from './store.m.js';
// #import {addApp, changeApp, removeApp, updateArcSupported} from './actions.m.js';
// clang-format on

cr.define('app_management.apiListener', function() {
  let initialized = false;

  async function init() {
    assert(!initialized);

    const {apps: initialApps} =
        await app_management.BrowserProxy.getInstance().handler.getApps();
    const initialState = app_management.util.createInitialState(initialApps);
    app_management.AppManagementStore.getInstance().init(initialState);

    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;

    callbackRouter.onAppAdded.addListener(onAppAdded);
    callbackRouter.onAppChanged.addListener(onAppChanged);
    callbackRouter.onAppRemoved.addListener(onAppRemoved);
    callbackRouter.onArcSupportChanged.addListener(onArcSupportChanged);

    initialized = true;
  }

  /**
   * @param {cr.ui.Action} action
   */
  function dispatch(action) {
    app_management.AppManagementStore.getInstance().dispatch(action);
  }

  /**
   * @param {App} app
   */
  function onAppAdded(app) {
    dispatch(app_management.actions.addApp(app));
  }

  /**
   * @param {App} app
   */
  function onAppChanged(app) {
    dispatch(app_management.actions.changeApp(app));
  }

  /**
   * @param {string} appId
   */
  function onAppRemoved(appId) {
    dispatch(app_management.actions.removeApp(appId));
  }

  /**
   * @param {boolean} isSupported
   */
  function onArcSupportChanged(isSupported) {
    dispatch(app_management.actions.updateArcSupported(isSupported));
  }

  init();

  // #cr_define_end
  return {};
});
