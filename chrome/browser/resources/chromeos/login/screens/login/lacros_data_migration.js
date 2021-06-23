// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for lacros data migration screen.
 */

Polymer({
  is: 'lacros-data-migration-element',

  behaviors: [OobeDialogHostBehavior, LoginScreenBehavior],

  ready() {
    this.initializeLoginScreen('LacrosDataMigrationScreen', {
      resetAllowed: false,
    });
  },
});
