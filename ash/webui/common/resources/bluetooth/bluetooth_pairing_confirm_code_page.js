// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via confirm passkey is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_pairing_confirm_code_page.html.js';
import {ButtonBarState, ButtonState} from './bluetooth_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingConfirmCodePageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingConfirmCodePageElement extends
    SettingsBluetoothPairingConfirmCodePageElementBase {
  static get is() {
    return 'bluetooth-pairing-confirm-code-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {string} */
      code: {
        type: String,
        value: '',
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.ENABLED,
        },
      },
    };
  }

  /**
   * @param {!Event} event
   * @private
   */
  onPairClicked_(event) {
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('confirm-code', {
      bubbles: true,
      composed: true,
    }));
  }
}

customElements.define(
    SettingsBluetoothPairingConfirmCodePageElement.is,
    SettingsBluetoothPairingConfirmCodePageElement);
