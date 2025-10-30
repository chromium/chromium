// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `security-page-feature-row` is a toggle with an expand button that
 * controls a supplied preference and also allows for expanding and
 * collapsing so a user can learn more about a setting.
 */
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './security_page_feature_row.html.js';

export class SecurityPageFeatureRowElement extends PolymerElement {
  static get is() {
    return 'security-page-feature-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded: {
        type: Boolean,
        notify: true,
        value: false,
      },

      label: String,

      /* The Preference associated with the feature row. */
      pref: Object,

      subLabel: String,

      numericUncheckedValues: Array,

      stateTextMap: Object,

      /* The computed string label for the current pref state. */
      currentStateLabel_: {
        type: String,
        computed: 'computeCurrentStateLabel_(pref.value, stateTextMap)',
      },
    };
  }

  declare expanded: boolean;
  declare label: string;
  declare pref: chrome.settingsPrivate.PrefObject;
  declare subLabel: string;
  declare numericUncheckedValues: number[];
  declare stateTextMap: Record<string, string>;
  declare private currentStateLabel_: string;

  private computeCurrentStateLabel_(): string {
    if (this.stateTextMap && this.stateTextMap[this.pref.value] !== undefined) {
      return this.stateTextMap[this.pref.value];
    }
    // Return an empty string if no mapping is found
    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'security-page-feature-row': SecurityPageFeatureRowElement;
  }
}

customElements.define(SecurityPageFeatureRowElement.is, SecurityPageFeatureRowElement);
