// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup screen for login/OOBE.
 */

Polymer({
  is: 'multidevice-setup-element',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior],

  ready() {
    this.initializeLoginScreen('MultiDeviceSetupScreen', {});
  },


  get defaultControl() {
    return this.$.impl;
  },

  updateLocalizedContent() {
    this.$.impl.updateLocalizedContent();
  },

  onBeforeShow() {
    if (loadTimeData.valueExists('newLayoutEnabled') &&
        loadTimeData.getBoolean('newLayoutEnabled')) {
      document.documentElement.setAttribute('new-layout', '');
    } else {
      document.documentElement.removeAttribute('new-layout');
    }
  },
});
