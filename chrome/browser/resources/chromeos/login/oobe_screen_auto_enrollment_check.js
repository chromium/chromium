// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Auto-enrollment check screen implementation.
 */

Polymer({
  is: 'auto-enrollment-check-element',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior, OobeDialogHostBehavior],

  ready() {
    this.initializeLoginScreen('AutoEnrollmentCheckScreen', {
      resetAllowed: true,
    });
  },
});
