// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via display passkey or PIN is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {assert} from '//resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_pairing_enter_code_page.html.js';
import {ButtonBarState, ButtonState} from './bluetooth_types.js';

// Pairing passkey can be a maximum of 16 characters while pairing code a max
// of  6 digits. This is used to check that the passed code is less than or
// equal to the max possible value.
const MAX_CODE_LENGTH = 16;
/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingEnterCodeElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingEnterCodeElement extends
    SettingsBluetoothPairingEnterCodeElementBase {
  static get is() {
    return 'bluetooth-pairing-enter-code-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * @type {string}
       */
      deviceName: {
        type: String,
        value: '',
      },

      /** @type {string} */
      code: {
        type: String,
        value: '',
      },

      /** @type {number} */
      numKeysEntered: {
        type: Number,
        value: 0,
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      /**
       * Array representation of |code|.
       * @private {!Array<string>}
       */
      keys_: {
        type: Array,
        computed: 'computeKeys_(code)',
      },
    };
  }

  /**
   * @return {!Array<string>}
   * @private
   */
  computeKeys_() {
    if (!this.code) {
      return [];
    }

    assert(this.code.length <= MAX_CODE_LENGTH);
    return this.code.split('');
  }

  /**
   * @param {number} index
   * @return {string}
   */
  getKeyAt_(index) {
    return this.keys_[index];
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getKeyClass_(index) {
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

  /**
   * @return {string}
   * @private
   */
  getEnterClass_() {
    if (!this.keys_ || !this.numKeysEntered) {
      return '';
    }

    if (this.numKeysEntered >= this.keys_.length) {
      return 'next';
    }

    return '';
  }

  /**
   * @private
   * @return {string}
   */
  getMessage_() {
    return this.i18n('bluetoothPairingEnterKeys', this.deviceName);
  }
}

customElements.define(
    SettingsBluetoothPairingEnterCodeElement.is,
    SettingsBluetoothPairingEnterCodeElement);
