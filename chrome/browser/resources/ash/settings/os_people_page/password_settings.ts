// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {fireAuthTokenInvalidEvent} from 'chrome://resources/ash/common/quick_unlock/utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {AuthFactor, AuthFactorConfig, ConfigureResult, FactorObserverReceiver, PasswordFactorEditor} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
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

      hasCryptohomePinV2_: {
        type: Boolean,
        value: false,
      },
    };
  }

  authToken: string|null;
  private hasCryptohomePinV2_: boolean;
  private hasGaiaPassword_: boolean;
  private hasLocalPassword_: boolean;

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
      case AuthFactor.kCryptohomePinV2:
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
    // clang-format off
    const [{configured: hasGaiaPassword},
      {configured: hasLocalPassword},
      {configured: hasCryptohomePinV2}] =
        await Promise.all([
          afc.isConfigured(authToken, AuthFactor.kGaiaPassword),
          afc.isConfigured(authToken, AuthFactor.kLocalPassword),
          afc.isConfigured(authToken, AuthFactor.kCryptohomePinV2),
        ]);
    // clang-format on
    this.hasGaiaPassword_ = hasGaiaPassword;
    this.hasLocalPassword_ = hasLocalPassword;
    this.hasCryptohomePinV2_ = hasCryptohomePinV2;
  }

  private hasPassword_(): boolean {
    return this.hasGaiaPassword_ || this.hasLocalPassword_;
  }

  private hasNoPassword_(): boolean {
    return !this.hasPassword_();
  }

  private setLocalPasswordDialog(): SettingsSetLocalPasswordDialogElement {
    const el = this.shadowRoot!.getElementById('setLocalPasswordDialog');
    assert(el instanceof SettingsSetLocalPasswordDialogElement);
    return el;
  }

  private openSetLocalPasswordDialog_(): void {
    this.setLocalPasswordDialog().showModal();
  }

  private moreButton_(): CrIconButtonElement {
    const moreButton = this.shadowRoot!.querySelector('#moreButton');
    assert(moreButton instanceof CrIconButtonElement);
    return moreButton;
  }

  private moreMenu_(): CrActionMenuElement {
    const moreMenu = this.shadowRoot!.querySelector('#moreMenu');
    assert(moreMenu instanceof CrActionMenuElement);
    return moreMenu;
  }


  private onMoreButtonClicked_(event: Event): void {
    event.preventDefault();  // Prevent default browser action (navigation).

    const moreButton = this.moreButton_();
    const moreMenu = this.moreMenu_();
    moreMenu.showAt(moreButton);
  }

  private isRemoveAllowed_(
      hasCryptohomePinV2: boolean, hasGaiaPassword: boolean,
      hasLocalPassword: boolean): boolean {
    return hasCryptohomePinV2 && (hasGaiaPassword || hasLocalPassword);
  }

  private async onRemovePasswordButtonClicked_(): Promise<void> {
    if (typeof this.authToken !== 'string') {
      console.error('Tried to remove password with expired token.');
    } else {
      const { result } =
      await PasswordFactorEditor.getRemote().removePassword(this.authToken);
      switch (result) {
        case ConfigureResult.kSuccess:
          break;
        case ConfigureResult.kInvalidTokenError:
          fireAuthTokenInvalidEvent(this);
          break;
        case ConfigureResult.kFatalError:
          console.error('Error removing Password');
          break;
      }
    }

    // We always close the "more" menu, even when removePassword call didn't
    // work: If the menu isn't closed but not attached anymore, then the user
    // can't interact with the whole settings UI at all anymore.
    const moreMenu = this.moreMenu_();
    if (moreMenu) {
      moreMenu.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPasswordSettingsElement.is]: SettingsPasswordSettingsElement;
  }
}

customElements.define(
    SettingsPasswordSettingsElement.is, SettingsPasswordSettingsElement);
