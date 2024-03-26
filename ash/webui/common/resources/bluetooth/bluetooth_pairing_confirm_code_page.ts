// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via confirm passkey is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_pairing_confirm_code_page.html.js';
import {ButtonBarState, ButtonState} from './bluetooth_types.js';

const SettingsBluetoothPairingConfirmCodePageElementBase =
    I18nMixin(PolymerElement);

export class SettingsBluetoothPairingConfirmCodePageElement extends
    SettingsBluetoothPairingConfirmCodePageElementBase {
  static get is() {
    return 'bluetooth-pairing-confirm-code-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      code: {
        type: String,
        value: '',
      },

      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.ENABLED,
        },
      },
    };
  }

  code: string;
  private buttonBarState_: ButtonBarState;

  private onPairClicked_(event: Event): void {
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('confirm-code', {
      bubbles: true,
      composed: true,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothPairingConfirmCodePageElement.is]:
    SettingsBluetoothPairingConfirmCodePageElement;
  }
}

customElements.define(
    SettingsBluetoothPairingConfirmCodePageElement.is,
    SettingsBluetoothPairingConfirmCodePageElement);
