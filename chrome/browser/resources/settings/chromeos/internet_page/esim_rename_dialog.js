// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to rename eSIM profile name
 */

import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_setup_icons.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {getESimProfile} from 'chrome://resources/cr_components/chromeos/cellular_setup/esim_manager_utils.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {number} */
const MAX_INPUT_LENGTH = 20;

/** @type {number} */
const MIN_INPUT_LENGTH = 1;

/** @type {RegExp} */
const EMOJI_REGEX_EXP =
    /(\u00a9|\u00ae|[\u2000-\u3300]|\ud83c[\ud000-\udfff]|\ud83d[\ud000-\udfff]|\ud83e[\ud000-\udfff])/gi;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const EsimRenameDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class EsimRenameDialogElement extends EsimRenameDialogElementBase {
  static get is() {
    return 'esim-rename-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Used to reference the MAX_INPUT_LENGTH constant in HTML. */
      MAX_INPUT_LENGTH: {
        type: Number,
        value: MAX_INPUT_LENGTH,
      },

      /** @type {?OncMojo.NetworkStateProperties} */
      networkState: {
        type: Object,
        value: null,
      },

      /** @type {boolean} */
      showCellularDisconnectWarning: {
        type: Boolean,
        value: false,
      },

      /** @private {string} */
      esimProfileName_: {
        type: String,
        value: '',
        observer: 'onEsimProfileNameChanged_',
      },

      /** @private {string} */
      errorMessage_: {
        type: String,
        value: '',
      },

      /** @private {boolean} */
      isRenameInProgress_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      isInputInvalid_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /** @private {?ash.cellularSetup.mojom.ESimProfileRemote} */
    this.esimProfileRemote_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.init_();
  }

  /** @private */
  async init_() {
    if (!(this.networkState &&
          this.networkState.type ===
              chromeos.networkConfig.mojom.NetworkType.kCellular)) {
      return;
    }
    this.esimProfileRemote_ =
        await getESimProfile(this.networkState.typeState.cellular.iccid);
    // Fail gracefully if init is incomplete, see crbug/1194729.
    if (!this.esimProfileRemote_) {
      this.errorMessage_ = this.i18n('eSimRenameProfileDialogError');
    }
    this.esimProfileName_ = this.networkState.name;

    if (!this.errorMessage_) {
      this.shadowRoot.querySelector('#eSimprofileName').focus();
    }
  }

  /**
   * Converts a mojoBase.mojom.String16 to a JavaScript String.
   * @param {?mojoBase.mojom.String16} str
   * @return {string}
   */
  convertString16ToJSString_(str) {
    return str.data.map(ch => String.fromCodePoint(ch)).join('');
  }

  /**
   * @param {Event} event
   * @private
   */
  async onRenameDialogDoneTap_(event) {
    if (this.errorMessage_) {
      this.$.profileRenameDialog.close();
      return;
    }

    this.isRenameInProgress_ = true;

    // The C++ layer uses std::u16string, which use 16 bit characters. JS
    // strings support either 8 or 16 bit characters, and must be converted
    // to an array of 16 bit character codes that match std::u16string.
    const name = {data: Array.from(this.esimProfileName_, c => c.charCodeAt())};

    this.esimProfileRemote_.setProfileNickname(name).then(response => {
      this.handleSetProfileNicknameResponse_(response.result);
    });
  }

  /**
   * @param {ash.cellularSetup.mojom.ESimOperationResult} result
   * @private
   */
  handleSetProfileNicknameResponse_(result) {
    this.isRenameInProgress_ = false;
    if (result === ash.cellularSetup.mojom.ESimOperationResult.kFailure) {
      const showErrorToastEvent = new CustomEvent('show-error-toast', {
        bubbles: true,
        composed: true,
        detail: this.i18n('eSimRenameProfileDialogError'),
      });
      this.dispatchEvent(showErrorToastEvent);
    }
    this.$.profileRenameDialog.close();
  }

  /**
   * @param {Event} event
   * @private
   */
  onCancelTap_(event) {
    this.$.profileRenameDialog.close();
  }

  /**
   * Observer for esimProfileName_ that sanitizes its value by removing any
   * Emojis and truncating it to MAX_INPUT_LENGTH. This method will be
   * recursively called until esimProfileName_ is fully sanitized.
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onEsimProfileNameChanged_(newValue, oldValue) {
    if (oldValue) {
      const sanitizedOldValue = oldValue.replace(EMOJI_REGEX_EXP, '');
      // If sanitizedOldValue.length > MAX_INPUT_LENGTH, the user attempted to
      // enter more than the max limit, this method was called and it was
      // truncated, and then this method was called one more time.
      this.isInputInvalid_ = sanitizedOldValue.length > MAX_INPUT_LENGTH;
    } else {
      this.isInputInvalid_ = false;
    }

    // Remove all Emojis from the name.
    const sanitizedProfileName =
        this.esimProfileName_.replace(EMOJI_REGEX_EXP, '');

    // Truncate the name to MAX_INPUT_LENGTH.
    this.esimProfileName_ = sanitizedProfileName.substring(0, MAX_INPUT_LENGTH);
  }

  /**
   * @param {boolean} isInputInvalid
   * @return {string}
   * @private
   */
  getInputInfoClass_(isInputInvalid) {
    return isInputInvalid ? 'error' : '';
  }

  /**
   * Returns a formatted string containing the current number of characters
   * entered in the input compared to the maximum number of characters allowed.
   * @param {string} esimProfileName
   * @return {string}
   * @private
   */
  getInputCountString_(esimProfileName) {
    // minimumIntegerDigits is 2 because we want to show a leading zero if
    // length is less than 10.
    return this.i18n(
        'eSimRenameProfileInputCharacterCount',
        esimProfileName.length.toLocaleString(
            /*locales=*/ undefined, {minimumIntegerDigits: 2}),
        MAX_INPUT_LENGTH.toLocaleString());
  }

  /**
   * @param {boolean} isRenameInProgress
   * @param {string} esimProfileName
   * @return {boolean}
   * @private
   */
  isDoneButtonDisabled_(isRenameInProgress, esimProfileName) {
    if (isRenameInProgress) {
      return true;
    }
    return esimProfileName.length < MIN_INPUT_LENGTH;
  }

  /**
   * @param {string} esimProfileName
   * @return {string}
   * @private
   */
  getDoneBtnA11yLabel_(esimProfileName) {
    return this.i18n('eSimRenameProfileDoneBtnA11yLabel', esimProfileName);
  }
}

customElements.define(EsimRenameDialogElement.is, EsimRenameDialogElement);
