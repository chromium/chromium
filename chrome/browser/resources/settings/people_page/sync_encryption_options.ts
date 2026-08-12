// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
// <if expr="is_chromeos">
import type {CrRadioGroupElement} from '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
// </if>

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';

import {getCss} from './sync_encryption_options.css.js';
import {getHtml} from './sync_encryption_options.html.js';

/**
 * Names of the radio buttons which allow the user to choose their encryption
 * mechanism.
 */
enum RadioButtonNames {
  ENCRYPT_WITH_GOOGLE = 'encrypt-with-google',
  ENCRYPT_WITH_PASSPHRASE = 'encrypt-with-passphrase',
}

export type SyncEncryptionOptionsElement = SettingsSyncEncryptionOptionsElement;

export class SettingsSyncEncryptionOptionsElement extends CrLitElement {
  static get is() {
    return 'settings-sync-encryption-options';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      syncPrefs: {type: Object},
      syncStatus: {type: Object},
      existingPassphraseLabel: {type: String},

      /**
       * Whether the "create passphrase" inputs should be shown. These inputs
       * give the user the opportunity to use a custom passphrase instead of
       * authenticating with their Google credentials.
       */
      creatingNewPassphrase_: {type: Boolean},

      /**
       * The passphrase input field value.
       */
      passphrase_: {type: String},

      /**
       * The passphrase confirmation input field value.
       */
      confirmation_: {type: String},

      disableEncryptionOptions_: {type: Boolean},
    };
  }

  accessor syncPrefs: SyncPrefs|null = null;
  accessor syncStatus: SyncStatus|null = null;
  accessor existingPassphraseLabel: string = '';
  protected accessor creatingNewPassphrase_: boolean = false;
  protected accessor passphrase_: string = '';
  protected accessor confirmation_: string = '';
  protected accessor disableEncryptionOptions_: boolean = false;
  /**
   * Whether there's a setEncryptionPassphrase() call pending response, in
   * which case the component should wait before making a new call.
   */
  private isSettingEncryptionPassphrase_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('syncPrefs') ||
        changedProperties.has('syncStatus')) {
      this.disableEncryptionOptions_ = this.computeDisableEncryptionOptions_();
      if (this.disableEncryptionOptions_) {
        this.creatingNewPassphrase_ = false;
      }
    }
  }

  // <if expr="is_chromeos">
  /**
   * Returns the encryption options CrRadioGroupElement.
   */
  getEncryptionsRadioButtons(): CrRadioGroupElement|null {
    return this.shadowRoot.querySelector('cr-radio-group');
  }
  // </if>

  /**
   * Whether we should disable the radio buttons that allow choosing the
   * encryption options for Sync.
   * We disable the buttons if:
   * (a) full data encryption is enabled, or,
   * (b) full data encryption is not allowed (so far, only applies to
   * supervised accounts), or,
   * (c) current encryption keys are missing, or,
   * (d) the user is a supervised account.
   */
  private computeDisableEncryptionOptions_(): boolean {
    return !!(
        (this.syncPrefs &&
         (this.syncPrefs.encryptAllData ||
          !this.syncPrefs.customPassphraseAllowed ||
          this.syncPrefs.trustedVaultKeysRequired)) ||
        (this.syncStatus && this.syncStatus.supervisedUser));
  }

  protected onPassphraseValueChanged_(e: CustomEvent<{value: string}>) {
    this.passphrase_ = e.detail.value;
  }

  protected onConfirmationValueChanged_(e: CustomEvent<{value: string}>) {
    this.confirmation_ = e.detail.value;
  }

  /**
   * @return Whether the passphrase save button should be enabled.
   */
  protected isSaveNewPassphraseEnabled_(): boolean {
    return this.passphrase_ !== '' && this.confirmation_ !== '';
  }

  protected onNewPassphraseInputKeypress_(e: KeyboardEvent) {
    if (e.type === 'keypress' && e.key !== 'Enter') {
      return;
    }
    this.saveNewPassphrase_();
  }

  protected onSaveNewPassphraseClick_() {
    this.saveNewPassphrase_();
  }

  /**
   * Sends the newly created custom sync passphrase to the browser.
   */
  private saveNewPassphrase_() {
    assert(this.creatingNewPassphrase_);
    chrome.metricsPrivate.recordUserAction('Sync_SaveNewPassphraseClicked');

    if (this.isSettingEncryptionPassphrase_) {
      return;
    }

    // If a new password has been entered but it is invalid, do not send the
    // sync state to the API.
    if (!this.validateCreatedPassphrases_()) {
      return;
    }

    this.isSettingEncryptionPassphrase_ = true;
    SyncBrowserProxyImpl.getInstance()
        .setEncryptionPassphrase(this.passphrase_)
        .then(successfullySet => {
          // TODO(crbug.com/40725814): Rename the event, there is no change if
          // |successfullySet| is false. It should also mention 'encryption
          // passphrase' in its name.
          this.fire('passphrase-changed', {didChange: successfullySet});
          this.isSettingEncryptionPassphrase_ = false;
        });
  }

  protected onEncryptionRadioSelectedChanged_(
      event: CustomEvent<{value: string}>) {
    this.creatingNewPassphrase_ =
        event.detail.value === RadioButtonNames.ENCRYPT_WITH_PASSPHRASE;
  }

  /**
   * Computed binding returning the selected encryption radio button.
   */
  protected selectedEncryptionRadio_() {
    return this.syncPrefs?.encryptAllData || this.creatingNewPassphrase_ ?
        RadioButtonNames.ENCRYPT_WITH_PASSPHRASE :
        RadioButtonNames.ENCRYPT_WITH_GOOGLE;
  }

  /**
   * Checks the supplied passphrases to ensure that they are not empty and that
   * they match each other. Additionally, displays error UI if they are invalid.
   * @return Whether the check was successful (i.e., that the passphrases were
   *     valid).
   */
  private validateCreatedPassphrases_(): boolean {
    const emptyPassphrase = !this.passphrase_;
    const mismatchedPassphrase = this.passphrase_ !== this.confirmation_;

    const passphraseInput =
        this.shadowRoot.querySelector<CrInputElement>('#passphraseInput');
    if (passphraseInput) {
      passphraseInput.invalid = emptyPassphrase;
    }
    const passphraseConfirmationInput =
        this.shadowRoot.querySelector<CrInputElement>(
            '#passphraseConfirmationInput');
    if (passphraseConfirmationInput) {
      passphraseConfirmationInput.invalid =
          !emptyPassphrase && mismatchedPassphrase;
    }

    return !emptyPassphrase && !mismatchedPassphrase;
  }

  protected onLearnMoreClick_(event: Event) {
    if ((event.target as HTMLElement).tagName === 'A') {
      // Stop the propagation of events, so that clicking on links inside
      // checkboxes or radio buttons won't change the value.
      event.stopPropagation();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-encryption-options': SettingsSyncEncryptionOptionsElement;
  }
}

customElements.define(
    SettingsSyncEncryptionOptionsElement.is,
    SettingsSyncEncryptionOptionsElement);
