// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Kiosk install/launch app splash screen implementation.
 */

login.createScreen('ArcKioskSplashScreen', 'arc-kiosk-splash', function() {
  return {
    EXTERNAL_API: [
      'updateArcKioskMessage',
    ],

    /** @override */
    decorate: function() {},

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow: function(data) {
      this.updateApp(data['appInfo']);

      Oobe.getInstance().solidBackground = true;
    },

    /**
     * Event handler that is invoked just before the frame is hidden.
     */
    onBeforeHide: function() {
      Oobe.getInstance().solidBackground = false;
    },

    /**
     * Updates the app name and icon.
     * @param {Object} app Details of app being launched.
     */
    updateApp: function(app) {
      $('arc-splash-header').textContent = app.name;
      $('arc-splash-header').style.backgroundImage = 'url(' + app.iconURL + ')';
    },

    /**
     * Updates the message for the current ARC kiosk state.
     * @param {string} message Description for current state.
     */
    updateArcKioskMessage: function(message) {
      $('arc-splash-launch-text').textContent = message;
    }
  };
});
