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
import '../common/password_prompt_dialog/password_prompt_dialog.js';

import {LockScreenProgress, recordLockScreenProgress} from 'chrome://resources/ash/common/quick_unlock/lock_screen_constants.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockStateMixin} from '../lock_state_mixin.js';

import {getTemplate} from './lock_screen_password_prompt_dialog.html.js';

const SettingsLockScreenPasswordPromptDialogElementBase =
    LockStateMixin(PolymerElement);

class SettingsLockScreenPasswordPromptDialogElement extends
    SettingsLockScreenPasswordPromptDialogElementBase {
  static get is() {
    return 'settings-lock-screen-password-prompt-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  override connectedCallback(): void {
    super.connectedCallback();

    recordLockScreenProgress(LockScreenProgress.START_SCREEN_LOCK);
  }

  private onTokenObtained_(
      {detail}: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>): void {
    // The user successfully authenticated.
    recordLockScreenProgress(LockScreenProgress.ENTER_PASSWORD_CORRECTLY);
    const authTokenObtainedEvent = new CustomEvent(
        'auth-token-obtained', {bubbles: true, composed: true, detail});
    this.dispatchEvent(authTokenObtainedEvent);
  }

  /**
   * Looks up the translation id, which depends on PIN login support.
   */
  private selectPasswordPromptEnterPasswordString_(hasPinLogin: boolean):
      string {
    if (hasPinLogin) {
      return this.i18n('passwordPromptEnterPasswordLoginLock');
    }
    return this.i18n('passwordPromptEnterPasswordLock');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsLockScreenPasswordPromptDialogElement.is]:
        SettingsLockScreenPasswordPromptDialogElement;
  }
}

customElements.define(
    SettingsLockScreenPasswordPromptDialogElement.is,
    SettingsLockScreenPasswordPromptDialogElement);
