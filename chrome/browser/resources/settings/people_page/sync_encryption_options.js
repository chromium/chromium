// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer, html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import {SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from './sync_browser_proxy.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';


/**
 * Names of the radio buttons which allow the user to choose their encryption
 * mechanism.
 * @enum {string}
 */
const RadioButtonNames = {
  ENCRYPT_WITH_GOOGLE: 'encrypt-with-google',
  ENCRYPT_WITH_PASSPHRASE: 'encrypt-with-passphrase',
};

Polymer({
  is: 'settings-sync-encryption-options',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * @type {SyncPrefs}
     */
    syncPrefs: {
      type: Object,
      notify: true,
    },

    /** @type {SyncStatus} */
    syncStatus: Object,

    /**
     * Whether the "create passphrase" inputs should be shown. These inputs
     * give the user the opportunity to use a custom passphrase instead of
     * authenticating with their Google credentials.
     * @private
     */
    creatingNewPassphrase_: {
      type: Boolean,
      value: false,
    },

    /**
     * The passphrase input field value.
     * @private
     */
    passphrase_: {
      type: String,
      value: '',
    },

    /**
     * The passphrase confirmation input field value.
     * @private
     */
    confirmation_: {
      type: String,
      value: '',
    },

    /** @private */
    disableEncryptionOptions_: {
      type: Boolean,
      computed: 'computeDisableEncryptionOptions_(' +
          'syncPrefs, syncStatus)',
      observer: 'disableEncryptionOptionsChanged_',
    },
  },

  /**
   * Whether there's a setEncryptionPassphrase() call pending response, in which
   * case the component should wait before making a new call.
   * @private {boolean}
   */
  isSettingEncryptionPassphrase_: false,

  /**
   * Returns the encryption options CrRadioGroupElement.
   * @return {?CrRadioGroupElement}
   */
  getEncryptionsRadioButtons() {
    return /** @type {?CrRadioGroupElement} */ (
        this.$$('#encryptionRadioGroup'));
  },

  /**
   * Whether we should disable the radio buttons that allow choosing the
   * encryption options for Sync.
   * We disable the buttons if:
   * (a) full data encryption is enabled, or,
   * (b) full data encryption is not allowed (so far, only applies to
   * supervised accounts), or,
   * (c) current encryption keys are missing, or,
   * (d) the user is a supervised account.
   * @return {boolean}
   * @private
   */
  computeDisableEncryptionOptions_() {
    return !!(
        (this.syncPrefs &&
         (this.syncPrefs.encryptAllData ||
          !this.syncPrefs.encryptAllDataAllowed ||
          this.syncPrefs.trustedVaultKeysRequired)) ||
        (this.syncStatus && this.syncStatus.supervisedUser));
  },

  /** @private */
  disableEncryptionOptionsChanged_() {
    if (this.disableEncryptionOptions_) {
      this.creatingNewPassphrase_ = false;
    }
  },

  /**
   * @param {string} passphrase The passphrase input field value
   * @param {string} confirmation The passphrase confirmation input field value.
   * @return {boolean} Whether the passphrase save button should be enabled.
   * @private
   */
  isSaveNewPassphraseEnabled_(passphrase, confirmation) {
    return passphrase !== '' && confirmation !== '';
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onNewPassphraseInputKeypress_(e) {
    if (e.type === 'keypress' && e.key !== 'Enter') {
      return;
    }
    this.saveNewPassphrase_();
  },

  /** @private */
  onSaveNewPassphraseClick_() {
    this.saveNewPassphrase_();
  },

  /**
   * Sends the newly created custom sync passphrase to the browser.
   * @private
   */
  saveNewPassphrase_() {
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
          // TODO(crbug.com/1139060): Rename the event, there is no change if
          // |successfullySet| is false. It should also mention 'encryption
          // passphrase' in its name.
          this.fire('passphrase-changed', {didChange: successfullySet});
          this.isSettingEncryptionPassphrase_ = false;
        });
  },

  /**
   * Called when the encryption
   * @param {!CustomEvent<{value: string}>} event
   * @private
   */
  onEncryptionRadioSelectionChanged_(event) {
    this.creatingNewPassphrase_ =
        event.detail.value === RadioButtonNames.ENCRYPT_WITH_PASSPHRASE;
  },

  /**
   * Computed binding returning the selected encryption radio button.
   * @private
   */
  selectedEncryptionRadio_() {
    return this.syncPrefs.encryptAllData || this.creatingNewPassphrase_ ?
        RadioButtonNames.ENCRYPT_WITH_PASSPHRASE :
        RadioButtonNames.ENCRYPT_WITH_GOOGLE;
  },

  /**
   * Checks the supplied passphrases to ensure that they are not empty and that
   * they match each other. Additionally, displays error UI if they are invalid.
   * @return {boolean} Whether the check was successful (i.e., that the
   *     passphrases were valid).
   * @private
   */
  validateCreatedPassphrases_() {
    const emptyPassphrase = !this.passphrase_;
    const mismatchedPassphrase = this.passphrase_ !== this.confirmation_;

    this.$$('#passphraseInput').invalid = emptyPassphrase;
    this.$$('#passphraseConfirmationInput').invalid =
        !emptyPassphrase && mismatchedPassphrase;

    return !emptyPassphrase && !mismatchedPassphrase;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreClick_(event) {
    if (event.target.tagName === 'A') {
      // Stop the propagation of events, so that clicking on links inside
      // checkboxes or radio buttons won't change the value.
      event.stopPropagation();
    }
  },
});
