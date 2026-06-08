// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {browserProxyFactory} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createInitialState} from 'chrome://resources/cr_components/app_management/util.js';

import {addApp, changeApp, removeApp} from './actions.js';
import type {AppManagementActions} from './actions.js';
import {AppManagementStore} from './store.js';

let initialized = false;

export async function initStoreAndListeners(): Promise<void> {
  if (initialized) {
    return;
  }
  initialized = true;

  // Call two async functions and wait for both of them.
  const getAppsPromise = browserProxyFactory.getInstance().handler.getApps();
  const getSubAppToParentMapPromise =
      browserProxyFactory.getInstance().handler.getSubAppToParentMap();

  const responses =
      await Promise.all([getAppsPromise, getSubAppToParentMapPromise]);

  const {apps: initialApps} = responses[0];
  const {subAppToParentMap: initialSubAppToParentMap} = responses[1];

  const initialState =
      createInitialState(initialApps, initialSubAppToParentMap);
  AppManagementStore.getInstance().init(initialState);

  const callbackRouter = browserProxyFactory.getInstance().callbackRouter;

  callbackRouter.onAppAdded.addListener(onAppAdded);
  callbackRouter.onAppChanged.addListener(onAppChanged);
  callbackRouter.onAppRemoved.addListener(onAppRemoved);
}

function dispatch(action: AppManagementActions): void {
  AppManagementStore.getInstance().dispatch(action);
}

function onAppAdded(app: App): void {
  dispatch(addApp(app));
}

function onAppChanged(app: App): void {
  dispatch(changeApp(app));
}

function onAppRemoved(appId: string): void {
  dispatch(removeApp(appId));
}
