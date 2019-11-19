// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enable developer features screen implementation.
 */

login.createScreen('EnableDebuggingScreen', 'debugging', function() {
  return {

    /* Possible UI states of the enable debugging screen. */
    UI_STATE:
        {ERROR: -1, NONE: 0, REMOVE_PROTECTION: 1, SETUP: 2, WAIT: 3, DONE: 4},

    EXTERNAL_API: ['updateState'],

    /** @override */
    decorate: function() {
      $('enable-debugging-help-link')
          .addEventListener('click', function(event) {
            chrome.send('enableDebuggingOnLearnMore');
          });

      var password = $('enable-debugging-password');
      var password2 = $('enable-debugging-password2');
      password.addEventListener('input', this.onPasswordChanged_.bind(this));
      password2.addEventListener('input', this.onPasswordChanged_.bind(this));
      password.placeholder =
          loadTimeData.getString('enableDebuggingPasswordLabel');
      password2.placeholder =
          loadTimeData.getString('enableDebuggingConfirmPasswordLabel');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];
      var rootfsRemoveButton = this.ownerDocument.createElement('button');
      rootfsRemoveButton.id = 'debugging-remove-protection-button';
      rootfsRemoveButton.textContent =
          loadTimeData.getString('enableDebuggingRemoveButton');
      rootfsRemoveButton.addEventListener('click', function(e) {
        chrome.send('enableDebuggingOnRemoveRootFSProtection');
        e.stopPropagation();
      });
      buttons.push(rootfsRemoveButton);

      var enableButton = this.ownerDocument.createElement('button');
      enableButton.id = 'debugging-enable-button';
      enableButton.textContent =
          loadTimeData.getString('enableDebuggingEnableButton');
      enableButton.addEventListener('click', function(e) {
        chrome.send(
            'enableDebuggingOnSetup', [$('enable-debugging-password').value]);
        e.stopPropagation();
      });
      buttons.push(enableButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'debugging-cancel-button';
      cancelButton.textContent =
          loadTimeData.getString('enableDebuggingCancelButton');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('enableDebuggingOnCancel');
        e.stopPropagation();
      });
      buttons.push(cancelButton);

      var okButton = this.ownerDocument.createElement('button');
      okButton.id = 'debugging-ok-button';
      okButton.textContent = loadTimeData.getString('enableDebuggingOKButton');
      okButton.addEventListener('click', function(e) {
        chrome.send('enableDebuggingOnDone');
        e.stopPropagation();
      });
      buttons.push(okButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      if (this.state_ == this.UI_STATE.REMOVE_PROTECTION)
        return $('debugging-remove-protection-button');
      else if (this.state_ == this.UI_STATE.SETUP)
        return $('enable-debugging-password');
      else if (
          this.state_ == this.UI_STATE.DONE ||
          this.state_ == this.UI_STATE.ERROR) {
        return $('debugging-ok-button');
      }

      return $('debugging-cancel-button');
    },

    /**
     * Cancels the enable debugging screen and drops the user back to the
     * network settings.
     */
    cancel: function() {
      chrome.send('enableDebuggingOnCancel');
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      this.setDialogView_(this.UI_STATE.NONE);
    },

    onPasswordChanged_: function() {
      var enableButton = $('debugging-enable-button');
      var password = $('enable-debugging-password');
      var password2 = $('enable-debugging-password2');
      var pwd = password.value;
      var pwd2 = password2.value;
      enableButton.disabled =
          !((pwd.length == 0 && pwd2.length == 0) ||
            (pwd == pwd2 && pwd.length >= 4));
    },

    /**
     * Sets css style for corresponding state of the screen.
     * @param {number} state.
     * @private
     */
    setDialogView_: function(state) {
      this.state_ = state;
      this.classList.toggle(
          'remove-protection-view', state == this.UI_STATE.REMOVE_PROTECTION);
      this.classList.toggle('setup-view', state == this.UI_STATE.SETUP);
      this.classList.toggle('wait-view', state == this.UI_STATE.WAIT);
      this.classList.toggle('done-view', state == this.UI_STATE.DONE);
      this.classList.toggle('error-view', state == this.UI_STATE.ERROR);
      this.defaultControl.focus();

      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
    },

    updateState: function(state) {
      this.setDialogView_(state);
    }
  };
});
