// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-device-name-dialog' allows editing of the device display name
 * when using Nearby Share.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {DeviceNameValidationResult} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNearbyShareSettings} from '../../shared/nearby_share_settings.js';
import {NearbySettings} from '../../shared/nearby_share_settings_behavior.js';

import {getTemplate} from './nearby_share_device_name_dialog.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyShareDeviceNameDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class NearbyShareDeviceNameDialogElement extends
    NearbyShareDeviceNameDialogElementBase {
  static get is() {
    return 'nearby-share-device-name-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {NearbySettings} */
      settings: {
        type: Object,
      },

      /** @type {string} */
      errorMessage: {
        type: String,
        value: '',
      },
    };
  }

  connectedCallback() {
    super.connectedCallback();

    this.open();
  }

  open() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  close() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  }

  /** @private */
  onDeviceNameInput_() {
    getNearbyShareSettings()
        .validateDeviceName(this.getEditInputValue_())
        .then((result) => {
          this.updateErrorMessage_(result.result);
        });
  }

  /** @private */
  onCancelClick_() {
    this.close();
  }

  /** @private */
  onSaveClick_() {
    getNearbyShareSettings()
        .setDeviceName(this.getEditInputValue_())
        .then((result) => {
          this.updateErrorMessage_(result.result);
          if (result.result === DeviceNameValidationResult.kValid) {
            this.close();
          }
        });
  }

  /**
   * @private
   *
   * @param {!DeviceNameValidationResult} validationResult The
   *     error status from validating the provided device name.
   */
  updateErrorMessage_(validationResult) {
    switch (validationResult) {
      case DeviceNameValidationResult.kErrorEmpty:
        this.errorMessage = this.i18n('nearbyShareDeviceNameEmptyError');
        break;
      case DeviceNameValidationResult.kErrorTooLong:
        this.errorMessage = this.i18n('nearbyShareDeviceNameTooLongError');
        break;
      case DeviceNameValidationResult.kErrorNotValidUtf8:
        this.errorMessage =
            this.i18n('nearbyShareDeviceNameInvalidCharactersError');
        break;
      default:
        this.errorMessage = '';
        break;
    }
  }

  /**
   * @private
   *
   * @return {!string}
   */
  getEditInputValue_() {
    return this.shadowRoot.querySelector('cr-input').value;
  }

  /**
   * @private
   *
   * @param {!string} errorMessage The error message.
   * @return {boolean} Whether or not the error message exists.
   */
  hasErrorMessage_(errorMessage) {
    return errorMessage !== '';
  }
}

customElements.define(
    NearbyShareDeviceNameDialogElement.is, NearbyShareDeviceNameDialogElement);
