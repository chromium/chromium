// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createEmptyState} from 'chrome://resources/cr_components/app_management/util.js';
import {Store} from 'chrome://resources/js/cr/ui/store.js';

import {reduceAction} from './reducers.js';

/**
 * @fileoverview A singleton datastore for the App Management page. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

export class AppManagementStore extends Store {
  constructor() {
    super(createEmptyState(), reduceAction);
  }

  /** @return {!AppManagementStore} */
  static getInstance() {
    return instance || (instance = new AppManagementStore());
  }

  /** @param {!AppManagementStore} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?AppManagementStore} */
let instance = null;
