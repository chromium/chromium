// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe reset screen implementation.
 */

login.createScreen('AutolaunchScreen', 'autolaunch', function() {
  return {
    EXTERNAL_API: ['updateApp', 'confirmAutoLaunchForTesting'],

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var confirmButton = this.ownerDocument.createElement('button');
      confirmButton.id = 'autolaunch-confirm-button';
      confirmButton.textContent =
          loadTimeData.getString('autolaunchConfirmButton');
      confirmButton.addEventListener('click', function(e) {
        chrome.send('autolaunchOnConfirm');
        e.stopPropagation();
      });
      buttons.push(confirmButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'autolaunch-cancel-button';
      cancelButton.textContent =
          loadTimeData.getString('autolaunchCancelButton');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('autolaunchOnCancel');
        e.stopPropagation();
      });
      buttons.push(cancelButton);
      return buttons;
    },

    /**
     * Event handler invoked when the page is shown and ready.
     */
    onBeforeShow: function() {
      chrome.send('autolaunchVisible');
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('autolaunch-cancel-button');
    },

    /**
     * Cancels the reset and drops the user back to the login screen.
     */
    cancel: function() {
      chrome.send('autolaunchOnCancel');
    },

    /**
     * Sets app to be displayed in the auto-launch warning.
     * @param {!Object} app An dictionary with app info.
     */
    updateApp: function(app) {
      if (app.appIconUrl && app.appIconUrl.length)
        $('autolaunch-app-icon').src = app.appIconUrl;

      $('autolaunch-app-name').innerText = app.appName;
    },

    /**
     * Initiates confirm/cancel response for testing.
     * @param {boolean} confirm True if the screen should confirm auto-launch.
     */
    confirmAutoLaunchForTesting: function(confirm) {
      var button = confirm ? $('autolaunch-confirm-button') :
                             $('autolaunch-cancel-button');
      var clickEvent = document.createEvent('Event');
      clickEvent.initEvent('click', true, true);
      button.dispatchEvent(clickEvent);
    }
  };
});
