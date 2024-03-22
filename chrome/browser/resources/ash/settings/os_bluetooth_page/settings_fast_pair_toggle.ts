// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Reusable toggle that turns Fast Pair on and off.
 */

import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getTemplate} from './settings_fast_pair_toggle.html.js';

const SettingsFastPairToggleElementBase = PrefsMixin(PolymerElement);

class SettingsFastPairToggleElement extends SettingsFastPairToggleElementBase {
  static get is() {
    return 'settings-fast-pair-toggle' as const;
  }

  static get template() {
    return getTemplate();
  }

  override focus(): void {
    this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                        '#toggle')!.focus();
  }

  static get properties() {
    return {
      /**
       * This property reflects the state of the Bluetooth toggle, which we use
       * to determine if Fast Pair should be disabled (greyed out) or not.
       */
      bluetoothToggleOnOff: {
        type: Boolean,
        observer: 'onBluetoothToggleOnOffChanged_',
      },
    };
  }

  bluetoothToggleOnOff: boolean;

  /**
   * When Bluetooth is toggled off, we set the Fast Pair toggle to off
   * disabled (greyed out). When Bluetooth is toggled on, we reset the
   * checked value to the value of the pref. Note that turning the Fast Pair
   * toggle off here is UI only and doesn't impact the value of the pref.
   * @private
   */
  private onBluetoothToggleOnOffChanged_(): void {
    const fastPairToggle =
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>('#toggle')!;

    if (this.bluetoothToggleOnOff) {
      // The Fast Pair pref can sometimes be undefined in tests.
      if (fastPairToggle.pref === undefined) {
        fastPairToggle.checked = false;
      } else {
        fastPairToggle.resetToPrefValue();
      }

      fastPairToggle.disabled = false;
      return;
    }

    fastPairToggle.checked = false;
    fastPairToggle.disabled = true;
  }
}

customElements.define(
    SettingsFastPairToggleElement.is, SettingsFastPairToggleElement);

declare global {
  interface HTMLElementTagNameMap {
    [SettingsFastPairToggleElement.is]: SettingsFastPairToggleElement;
  }
}
