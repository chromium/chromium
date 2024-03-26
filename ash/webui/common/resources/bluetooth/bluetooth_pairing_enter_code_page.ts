// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via display passkey or PIN is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_pairing_enter_code_page.html.js';
import {ButtonBarState, ButtonState} from './bluetooth_types.js';

// Pairing passkey can be a maximum of 16 characters while pairing code a max
// of  6 digits. This is used to check that the passed code is less than or
// equal to the max possible value.
const MAX_CODE_LENGTH: number = 16;

const SettingsBluetoothPairingEnterCodeElementBase = I18nMixin(PolymerElement);

export class SettingsBluetoothPairingEnterCodeElement extends
    SettingsBluetoothPairingEnterCodeElementBase {
  static get is() {
    return 'bluetooth-pairing-enter-code-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      deviceName: {
        type: String,
        value: '',
      },

      code: {
        type: String,
        value: '',
      },

      numKeysEntered: {
        type: Number,
        value: 0,
      },

      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      keys_: {
        type: Array,
        computed: 'computeKeys_(code)',
      },
    };
  }

  deviceName: string;
  code: string;
  numKeysEntered: number;
  private buttonBarState_: ButtonBarState;
  private keys_: string[];

  override focus(): void {
    super.focus();
    const elem = this.shadowRoot?.querySelector('bluetooth-base-page');
    if (elem) {
      elem.focus();
    }
  }

  private computeKeys_(): string[] {
    if (!this.code) {
      return [];
    }

    assert(this.code.length <= MAX_CODE_LENGTH);
    return this.code.split('');
  }

  private getKeyAt_(index: number): string {
    return this.keys_[index];
  }

  private getKeyClass_(index: number): string {
    if (!this.keys_ || !this.numKeysEntered) {
      return '';
    }

    if (index === this.numKeysEntered) {
      return 'next';
    } else if (index < this.numKeysEntered) {
      return 'typed';
    }

    return '';
  }

  private getEnterClass_(): string {
    if (!this.keys_ || !this.numKeysEntered) {
      return '';
    }

    if (this.numKeysEntered >= this.keys_.length) {
      return 'next';
    }

    return '';
  }

  private getMessage_(): string {
    return this.i18n('bluetoothPairingEnterKeys', this.deviceName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothPairingEnterCodeElement.is]: SettingsBluetoothPairingEnterCodeElement;
  }
}

customElements.define(
    SettingsBluetoothPairingEnterCodeElement.is,
    SettingsBluetoothPairingEnterCodeElement);
