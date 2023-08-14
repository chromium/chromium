// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {AuthFactor, AuthFactorConfig, FactorObserverReceiver} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_settings.html.js';

enum PasswordType {
  LOCAL = 'local',
  GAIA = 'gaia',
}

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

      // The purpose of this attribute is to allow us to observe when it
      // changes, i.e., when the user selects a different password type.
      selectedPasswordType_: {
        type: String,
        value: null,
        observer: 'onSelectedPasswordTypeChanged_',
      },
    };
  }

  authToken: string|null;
  private hasGaiaPassword_: boolean;
  private hasLocalPassword_: boolean;
  private selectedPasswordType_: PasswordType|null;

  override ready(): void {
    super.ready();
    // Register observer for auth factor updates.
    // TODO(crbug/1321440): Are we leaking |this| here because we never remove
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

    this.selectedPasswordType_ = this.passwordType_();
  }

  private hasPassword_(): boolean {
    return this.hasGaiaPassword_ || this.hasLocalPassword_;
  }

  private hasNoPassword_(): boolean {
    return !this.hasPassword_();
  }

  /**
   * Computes the current |PasswordType| based on the values of
   * hasGaiaPassword_ and hasLocalPassword_.
   */
  private passwordType_(): PasswordType|null {
    // This control works only when there is at most one password.
    assert(!(this.hasGaiaPassword_ && this.hasLocalPassword_));

    if (this.hasGaiaPassword_) {
      return PasswordType.GAIA;
    }

    if (this.hasLocalPassword_) {
      return PasswordType.LOCAL;
    }

    return null;
  }

  private onSelectedPasswordTypeChanged_(): void {
    // Check if the selected value is really a valid |PasswordType|. Recall
    // that typescript's type system is unsound, so this fails at runtime if a
    // |name| attribute of a select element in the template has a value that is
    // not listed in |PasswordType|.
    assert(
        this.selectedPasswordType_ === null ||
        Object.values(PasswordType).includes(this.selectedPasswordType_));

    // The `selectedPasswordType_` must always match the actually
    // configured `passwordType_`, so immediately switch back to it.
    //
    // TODO(b/290916811): However, we should now start changing the actually
    // configured password type, e.g. switch from local to Gaia password, which
    // will eventually result into an auth factor change notification and
    // updatePasswordState_ being called, which then sets
    // `selectedPasswordType_`.
    this.selectedPasswordType_ = this.passwordType_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPasswordSettingsElement.is]: SettingsPasswordSettingsElement;
  }
}

customElements.define(
    SettingsPasswordSettingsElement.is, SettingsPasswordSettingsElement);
