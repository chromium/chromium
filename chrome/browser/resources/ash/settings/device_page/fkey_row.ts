// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'fkey-row' displays an fKey alongside a dropdown menu that allows users to
 * set a shortcut for remapping key events to F11/F12.
 */

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './fkey_row.html.js';
import {Fkey} from './input_device_settings_types.js';

const fKeyLabels = {
  [Fkey.F11]: loadTimeData.getString('f11KeyLabel'),
  [Fkey.F12]: loadTimeData.getString('f12KeyLabel'),
};

export class FkeyRowElement extends PolymerElement {
  static get is() {
    return 'fkey-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      key: {type: String},

      keyLabel: {
        type: String,
        computed: 'computeKeyLabel(key)',
      },

      pref: {
        type: Object,
      },
    };
  }

  key: Fkey;
  keyLabel: string;
  pref: chrome.settingsPrivate.PrefObject;

  private computeKeyLabel(): string {
    assert(this.key in fKeyLabels);
    return fKeyLabels[this.key];
  }
}


declare global {
  interface HTMLElementTagNameMap {
    [FkeyRowElement.is]: FkeyRowElement;
  }
}

customElements.define(FkeyRowElement.is, FkeyRowElement);
