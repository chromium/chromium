// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Password changed screen implementation.
 */

login.createScreen('PasswordChangedScreen', 'password-changed', function() {
  return {
    EXTERNAL_API: ['show'],

    gaiaPasswordChanged_: null,

    /** @override */
    decorate: function() {
      this.gaiaPasswordChanged_ = $('gaia-password-changed');
      this.gaiaPasswordChanged_.addEventListener(
          'cancel', this.cancel.bind(this));

      this.gaiaPasswordChanged_.addEventListener('passwordEnter', function(e) {
        chrome.send('migrateUserData', [e.detail.password]);
      });

      this.gaiaPasswordChanged_.addEventListener('proceedAnyway', function() {
        chrome.send('resyncUserData');
      });
    },

    /**
     * Cancels password migration and drops the user back to the login screen.
     */
    cancel: function() {
      if (!this.gaiaPasswordChanged_.disabled) {
        chrome.send(
            'cancelPasswordChangedFlow',
            [this.gaiaPasswordChanged_.email || '']);
      }
    },

    onAfterShow: function(data) {
      this.gaiaPasswordChanged_.focus();
    },

    /**
     * Show password changed screen.
     * @param {boolean} showError Whether to show the incorrect password error.
     */
    show: function(showError, email) {
      this.gaiaPasswordChanged_.reset();
      if (showError)
        this.gaiaPasswordChanged_.invalidate();
      if (email)
        this.gaiaPasswordChanged_.email = email;

      // We'll get here after the successful online authentication.
      Oobe.showScreen({id: SCREEN_PASSWORD_CHANGED});
      Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.PASSWORD_CHANGED);
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      this.gaiaPasswordChanged_.i18nUpdateLocale();
    },

  };
});
