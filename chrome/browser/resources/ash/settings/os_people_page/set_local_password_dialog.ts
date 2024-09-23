// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/auth_setup/set_local_password_input.js';

import {SetLocalPasswordInputElement} from 'chrome://resources/ash/common/auth_setup/set_local_password_input.js';
import {fireAuthTokenInvalidEvent} from 'chrome://resources/ash/common/quick_unlock/utils.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {ConfigureResult, PasswordFactorEditor} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './set_local_password_dialog.html.js';

export interface SettingsSetLocalPasswordDialogElement {
  $: {
    setPasswordInput: SetLocalPasswordInputElement,
    submitButton: CrButtonElement,
    dialog: CrDialogElement,
  };
}

export class SettingsSetLocalPasswordDialogElement extends PolymerElement {
  static get is() {
    return 'settings-set-local-password-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      authToken: {
        type: String,
        value: null,
      },
      showError_: {
        type: Boolean,
        value: false,
      },
      password_: {
        type: String,
        value: null,
      },
    };
  }

  authToken: string|null;
  private showError_: boolean;
  private password_: string|null;

  override ready(): void {
    super.ready();

    // Dynamic checks to make sure that our static type declaration about named
    // elements in the shadow DOM are actually true.
    assert(this.$.setPasswordInput instanceof SetLocalPasswordInputElement);
    assert(this.$.submitButton instanceof CrButtonElement);
    assert(this.$.dialog instanceof CrDialogElement);
  }

  showModal(): void {
    if (!this.$.dialog.open) {
      this.reset();
      this.$.dialog.showModal();
      this.$.setPasswordInput.focus();
    }
  }

  private reset(): void {
    this.$.setPasswordInput.reset();
    this.showError_ = false;
  }

  private async submit(): Promise<void> {
    this.showError_ = false;

    await this.$.setPasswordInput.validate();
    const value = this.$.setPasswordInput.value;
    if (value === null) {
      return;
    }

    const authToken = this.authToken;
    if (typeof authToken !== 'string') {
      fireAuthTokenInvalidEvent(this);
      return;
    }

    const {result} =
        await PasswordFactorEditor.getRemote().updateOrSetLocalPassword(
            authToken, value);
    switch (result) {
      case ConfigureResult.kSuccess:
        this.$.dialog.close();
        return;
      case ConfigureResult.kInvalidTokenError:
        fireAuthTokenInvalidEvent(this);
        return;
      case ConfigureResult.kFatalError:
        this.showError_ = true;
        console.error('Internal error while setting local password');
        return;
    }
  }

  private cancel(): void {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSetLocalPasswordDialogElement.is]:
        SettingsSetLocalPasswordDialogElement;
  }
}

customElements.define(
    SettingsSetLocalPasswordDialogElement.is,
    SettingsSetLocalPasswordDialogElement);
