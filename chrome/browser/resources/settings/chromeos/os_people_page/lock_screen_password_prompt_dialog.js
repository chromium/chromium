// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-lock-screen-password-prompt-dialog' displays a password prompt to
 * the user. If the user has authenticated the element fires
 * 'auth-token-obtained' event with |chrome.quickUnlockPrivate.TokenInfo|
 * object.
 *
 * Example:
 *
 * <settings-lock-screen-password-prompt-dialog
 *   id="lockScreenPasswordPrompt"
 * </settings-lock-screen-password-prompt-dialog>
 */
Polymer({
  is: 'settings-lock-screen-password-prompt-dialog',

  behaviors: [
    LockStateBehavior,
  ],

  properties: {
    /**
     * writeUma_ is a function that handles writing uma stats. It may be
     * overridden for tests.
     *
     * @type {Function}
     * @private
     */
    writeUma_: {
      type: Object,
      value() {
        return settings.recordLockScreenProgress;
      }
    },
  },

  /** @override */
  attached() {
    this.writeUma_(settings.LockScreenProgress.START_SCREEN_LOCK);
  },

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   */
  onTokenObtained_(e) {
    // The user successfully authenticated.
    this.writeUma_(settings.LockScreenProgress.ENTER_PASSWORD_CORRECTLY);
    this.fire('auth-token-obtained', e.detail);
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @return {string}
   * @private
   */
  selectPasswordPromptEnterPasswordString_(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('passwordPromptEnterPasswordLoginLock');
    }
    return this.i18n('passwordPromptEnterPasswordLock');
  },
});
