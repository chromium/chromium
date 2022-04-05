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
import '../../controls/password_prompt_dialog.js';

import {LockScreenProgress, recordLockScreenProgress} from '//resources/cr_components/chromeos/quick_unlock/lock_screen_constants.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenUnlockType, LockStateBehavior, LockStateBehaviorImpl} from './lock_state_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
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
        return recordLockScreenProgress;
      }
    },
  },

  /** @override */
  attached() {
    this.writeUma_(LockScreenProgress.START_SCREEN_LOCK);
  },

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   */
  onTokenObtained_(e) {
    // The user successfully authenticated.
    this.writeUma_(LockScreenProgress.ENTER_PASSWORD_CORRECTLY);
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
