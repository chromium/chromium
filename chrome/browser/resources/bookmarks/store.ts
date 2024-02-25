// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Store as CrUiStore} from 'chrome://resources/js/store.js';

import {reduceAction} from './reducers.js';
import type {BookmarksPageState} from './types.js';
import {createEmptyState} from './util.js';

/**
 * @fileoverview A singleton datastore for the Bookmarks page. Page state is
 * publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

export class Store extends CrUiStore<BookmarksPageState> {
  constructor() {
    super(createEmptyState(), reduceAction);
  }

  static getInstance(): Store {
    return instance || (instance = new Store());
  }

  static setInstance(obj: Store) {
    instance = obj;
  }
}

let instance: (Store|null) = null;
