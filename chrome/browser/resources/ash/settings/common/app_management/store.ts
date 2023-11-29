// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A singleton datastore for the App Management page. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createEmptyState} from 'chrome://resources/cr_components/app_management/util.js';
import {Store} from 'chrome://resources/js/store.js';

import {AppManagementActions} from './actions.js';
import {reduceAction} from './reducers.js';

export type AppMap = Record<string, App>;

export interface AppManagementPageState {
  apps: AppMap;
  selectedAppId: string|null;
  // Maps all apps to their parent's app ID. Apps without a parent are
  // not listed in this map.
  subAppToParentAppId: Record<string, string>;
}

let instance: AppManagementStore|null = null;

export class AppManagementStore extends
    Store<AppManagementPageState, AppManagementActions> {
  static getInstance(): AppManagementStore {
    return instance || (instance = new AppManagementStore());
  }

  static setInstanceForTesting(obj: AppManagementStore): void {
    instance = obj;
  }

  constructor() {
    super(createEmptyState(), reduceAction);
  }
}
