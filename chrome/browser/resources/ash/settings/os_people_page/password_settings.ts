// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {AuthFactor, AuthFactorConfig, FactorObserverReceiver} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_settings.html.js';
import {SettingsSetLocalPasswordDialogElement} from './set_local_password_dialog.js';

export class SettingsPasswordSettingsElement extends PolymerElement {
  static get is() {
    return 'settings-password-settings' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      authToken: {
        type: String,
        value: null,
        observer: 'updatePasswordState_',
      },

      hasGaiaPassword_: {
        type: Boolean,
        value: false,
      },

      hasLocalPassword_: {
        type: Boolean,
        value: false,
      },

      changePasswordFactorSetupEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('changePasswordFactorSetupEnabled');
        },
        readOnly: true,
      },
    };
  }

  authToken: string|null;
  private hasGaiaPassword_: boolean;
  private hasLocalPassword_: boolean;
  private changePasswordFactorSetupEnabled_: boolean;

  override ready(): void {
    super.ready();
    // Register observer for auth factor updates.
    // TODO(crbug.com/40223898): Are we leaking |this| here because we never remove
    // the observer? We could close the pipe with |$.close()|, but not clear
    // whether that removes all references to |receiver| and then eventually to
    // |this|.
    const receiver = new FactorObserverReceiver(this);
    const remote = receiver.$.bindNewPipeAndPassRemote();
    AuthFactorConfig.getRemote().observeFactorChanges(remote);
  }

  onFactorChanged(factor: AuthFactor): void {
    switch (factor) {
      case AuthFactor.kGaiaPassword:
      case AuthFactor.kLocalPassword:
        this.updatePasswordState_();
        break;
      default:
        return;
    }
  }

  /**
   * Fetches the state of the password factor and updates the corresponding
   * property.
   */
  private async updatePasswordState_(): Promise<void> {
    if (!this.authToken) {
      return;
    }
    const authToken = this.authToken;

    const afc = AuthFactorConfig.getRemote();
    const [{configured: hasGaiaPassword}, {configured: hasLocalPassword}] =
        await Promise.all([
          afc.isConfigured(authToken, AuthFactor.kGaiaPassword),
          afc.isConfigured(authToken, AuthFactor.kLocalPassword),
        ]);

    this.hasGaiaPassword_ = hasGaiaPassword;
    this.hasLocalPassword_ = hasLocalPassword;
  }

  private hasPassword_(): boolean {
    return this.hasGaiaPassword_ || this.hasLocalPassword_;
  }

  private hasNoPassword_(): boolean {
    return !this.hasPassword_();
  }

  private shouldSetupPassword_(): boolean {
    return this.hasNoPassword_() || this.canSwitchLocalPassword_();
  }

  private setLocalPasswordDialog(): SettingsSetLocalPasswordDialogElement {
    const el = this.shadowRoot!.getElementById('setLocalPasswordDialog');
    assert(el instanceof SettingsSetLocalPasswordDialogElement);
    return el;
  }

  private openSetLocalPasswordDialog_(): void {
    this.setLocalPasswordDialog().showModal();
  }

  private canSwitchLocalPassword_(): boolean {
    return this.hasGaiaPassword_ && this.changePasswordFactorSetupEnabled_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPasswordSettingsElement.is]: SettingsPasswordSettingsElement;
  }
}

customElements.define(
    SettingsPasswordSettingsElement.is, SettingsPasswordSettingsElement);
