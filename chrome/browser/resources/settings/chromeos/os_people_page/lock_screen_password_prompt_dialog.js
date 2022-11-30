// Copyright 2016 The Chromium Authors
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

import {LockScreenProgress, recordLockScreenProgress} from 'chrome://resources/ash/common/quick_unlock/lock_screen_constants.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockStateBehavior, LockStateBehaviorInterface} from './lock_state_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LockStateBehaviorInterface}
 */
const SettingsLockScreenPasswordPromptDialogElementBase =
    mixinBehaviors([LockStateBehavior], PolymerElement);

/** @polymer */
class SettingsLockScreenPasswordPromptDialogElement extends
    SettingsLockScreenPasswordPromptDialogElementBase {
  static get is() {
    return 'settings-lock-screen-password-prompt-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
        },
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.writeUma_(LockScreenProgress.START_SCREEN_LOCK);
  }

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   */
  onTokenObtained_({detail}) {
    // The user successfully authenticated.
    this.writeUma_(LockScreenProgress.ENTER_PASSWORD_CORRECTLY);
    const authTokenObtainedEvent = new CustomEvent(
        'auth-token-obtained', {bubbles: true, composed: true, detail});
    this.dispatchEvent(authTokenObtainedEvent);
  }

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
  }
}

customElements.define(
    SettingsLockScreenPasswordPromptDialogElement.is,
    SettingsLockScreenPasswordPromptDialogElement);
