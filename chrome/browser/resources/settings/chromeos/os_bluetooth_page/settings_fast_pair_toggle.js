// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Reusable toggle that turns Fast Pair on and off.
 */

import '../../controls/settings_toggle_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_fast_pair_toggle.html.js';

/** @polymer */
class SettingsFastPairToggleElement extends PolymerElement {
  static get is() {
    return 'settings-fast-pair-toggle';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  /** @override */
  focus() {
    this.shadowRoot.querySelector('#toggle').focus();
  }
}

customElements.define(
    SettingsFastPairToggleElement.is, SettingsFastPairToggleElement);
