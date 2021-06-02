// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Store as CrUiStore} from 'chrome://resources/js/cr/ui/store.m.js';
import {reduceAction} from './reducers.js';
import {BookmarksPageState} from './types.js';
import {createEmptyState} from './util.js';

/**
 * @fileoverview A singleton datastore for the Bookmarks page. Page state is
 * publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

/** @extends {CrUiStore<BookmarksPageState>} */
export class Store extends CrUiStore {
  constructor() {
    super(createEmptyState(), reduceAction);
  }

  /** @return {!Store} */
  static getInstance() {
    return instance || (instance = new Store());
  }

  /** @param {Store} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?Store} */
let instance = null;
