// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-device-name-dialog' allows editing of the device display name
 * when using Nearby Share.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import '../settings_shared.css.js';

import {getNearbyShareSettings} from '/shared/nearby_share_settings.js';
import {NearbySettings} from '/shared/nearby_share_settings_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {DeviceNameValidationResult} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_share_device_name_dialog.html.js';

interface NearbyShareDeviceNameDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const NearbyShareDeviceNameDialogElementBase = I18nMixin(PolymerElement);

class NearbyShareDeviceNameDialogElement extends
    NearbyShareDeviceNameDialogElementBase {
  static get is() {
    return 'nearby-share-device-name-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      settings: {
        type: Object,
      },
      errorMessage: {
        type: String,
        value: '',
      },
    };
  }

  settings: NearbySettings;
  errorMessage: string;

  override connectedCallback(): void {
    super.connectedCallback();

    this.open();
  }

  open(): void {
    const dialog = this.$.dialog;
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  close(): void {
    const dialog = this.$.dialog;
    if (dialog.open) {
      dialog.close();
    }
  }

  private async onDeviceNameInput_(): Promise<void> {
    const result = await getNearbyShareSettings().validateDeviceName(
        this.getEditInputValue_());
    this.updateErrorMessage_(result.result);
  }

  private onCancelClick_(): void {
    this.close();
  }

  private async onSaveClick_(): Promise<void> {
    const result =
        await getNearbyShareSettings().setDeviceName(this.getEditInputValue_());
    this.updateErrorMessage_(result.result);
    if (result.result === DeviceNameValidationResult.kValid) {
      this.close();
    }
  }

  /**
   * @param validationResult The error status from validating the provided
   *    device name.
   */
  private updateErrorMessage_(validationResult: DeviceNameValidationResult):
      void {
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

  private getEditInputValue_(): string {
    return this.shadowRoot!.querySelector('cr-input')!.value;
  }

  private hasErrorMessage_(errorMessage: string): boolean {
    return errorMessage !== '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyShareDeviceNameDialogElement.is]: NearbyShareDeviceNameDialogElement;
  }
}

customElements.define(
    NearbyShareDeviceNameDialogElement.is, NearbyShareDeviceNameDialogElement);
