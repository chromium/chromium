// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe reset screen implementation.
 */

login.createScreen('KioskEnableScreen', 'kiosk-enable', function() {
  return {
    EXTERNAL_API: ['enableKioskForTesting', 'onCompleted'],

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var confirmButton = this.ownerDocument.createElement('button');
      confirmButton.id = 'kiosk-enable-button';
      confirmButton.textContent = loadTimeData.getString('kioskEnableButton');
      confirmButton.addEventListener('click', function(e) {
        chrome.send('kioskOnEnable');
        e.stopPropagation();
      });
      buttons.push(confirmButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'kiosk-cancel-button';
      cancelButton.textContent = loadTimeData.getString('kioskCancelButton');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('kioskOnClose');
        e.stopPropagation();
      });
      buttons.push(cancelButton);

      var okButton = this.ownerDocument.createElement('button');
      okButton.id = 'kiosk-ok-button';
      okButton.hidden = true;
      okButton.textContent = loadTimeData.getString('kioskOKButton');
      okButton.addEventListener('click', function(e) {
        chrome.send('kioskOnClose');
        e.stopPropagation();
      });
      buttons.push(okButton);
      return buttons;
    },

    /**
     * Event handler invoked when the page is shown and ready.
     */
    onBeforeShow: function() {
      $('kiosk-enable-button').hidden = false;
      $('kiosk-cancel-button').hidden = false;
      $('kiosk-ok-button').hidden = true;
      $('kiosk-enable-details').textContent =
          loadTimeData.getString('kioskEnableWarningDetails');
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('kiosk-cancel-button');
    },

    /**
     * Cancels the reset and drops the user back to the login screen.
     */
    cancel: function() {
      chrome.send('kioskOnClose');
    },

    /**
     * Initiates enable/cancel response for testing.
     * @param {boolean} confirm True if the screen should confirm auto-launch.
     */
    enableKioskForTesting: function(confirm) {
      var button =
          confirm ? $('kiosk-enable-button') : $('kiosk-cancel-button');
      var clickEvent = document.createEvent('Event');
      clickEvent.initEvent('click', true, true);
      button.dispatchEvent(clickEvent);
    },

    /**
     * Updates completion message on the screen.
     * @param {boolean} success True if consumer kiosk was successfully enabled.
     */
    onCompleted: function(success) {
      $('kiosk-enable-button').hidden = true;
      $('kiosk-cancel-button').hidden = true;
      $('kiosk-ok-button').hidden = false;
      $('kiosk-enable-details').textContent = loadTimeData.getString(
          success ? 'kioskEnableSuccessMsg' : 'kioskEnableErrorMsg');
    }
  };
});
