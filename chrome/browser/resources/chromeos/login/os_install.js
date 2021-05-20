// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS install screen.
 */
Polymer({
  is: 'os-install-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  ready() {
    this.initializeLoginScreen('OsInstallScreen', {
      resetAllowed: true,
    });
  },
});
