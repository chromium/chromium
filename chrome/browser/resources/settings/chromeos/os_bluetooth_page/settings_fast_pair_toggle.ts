// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Reusable toggle that turns Fast Pair on and off.
 */

import '../../controls/settings_toggle_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';

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
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsFastPairToggleElement.is]: SettingsFastPairToggleElement;
  }
}

customElements.define(
    SettingsFastPairToggleElement.is, SettingsFastPairToggleElement);
