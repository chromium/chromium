// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {Store} from 'chrome://resources/js/cr/ui/store.m.js';
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// #import {createEmptyState} from './util.m.js';
// #import {reduceAction} from './reducers.m.js';
// clang-format on

/**
 * @fileoverview A singleton datastore for the App Management page. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

cr.define('app_management', function() {
  /* #export */ class AppManagementStore extends cr.ui.Store {
    constructor() {
      super(
          app_management.util.createEmptyState(), app_management.reduceAction);
    }
  }

  cr.addSingletonGetter(AppManagementStore);

  // #cr_define_end
  return {
    AppManagementStore: AppManagementStore,
  };
});
