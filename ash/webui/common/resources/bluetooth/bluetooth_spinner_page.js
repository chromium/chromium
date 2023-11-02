// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element which shows a loading spinner.
 */

import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import './bluetooth_base_page.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './bluetooth_spinner_page.html.js';
import {ButtonBarState, ButtonState} from './bluetooth_types.js';

/** @polymer */
export class SettingsBluetoothSpinnerPageElement extends PolymerElement {
  static get is() {
    return 'bluetooth-spinner-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.DISABLED,
        },
      },
    };
  }
}

customElements.define(
    SettingsBluetoothSpinnerPageElement.is,
    SettingsBluetoothSpinnerPageElement);
