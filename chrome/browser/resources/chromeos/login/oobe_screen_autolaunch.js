// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe reset screen implementation.
 */

Polymer({
  is: 'autolaunch-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'updateApp',
  ],

  properties: {
    appName_: {type: String, value: ''},
    appIconUrl_: {type: String, value: ''},
  },


  ready() {
    this.initializeLoginScreen('AutolaunchScreen', {
      resetAllowed: true,
    });
  },

  onConfirm_() {
    chrome.send('autolaunchOnConfirm');
  },

  onCancel_() {
    chrome.send('autolaunchOnCancel');
  },

  /**
   * Event handler invoked when the page is shown and ready.
   */
  onBeforeShow() {
    chrome.send('autolaunchVisible');
  },

  /**
   * Cancels the reset and drops the user back to the login screen.
   */
  cancel() {
    chrome.send('autolaunchOnCancel');
  },

  /**
   * Sets app to be displayed in the auto-launch warning.
   * @param {!Object} app An dictionary with app info.
   */
  updateApp(app) {
    this.appName_ = app.appName;
    if (app.appIconUrl && app.appIconUrl.length)
      this.appIconUrl_ = app.appIconUrl;
  },
});
