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
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './security_page_feature_row.html.js';

export interface SecurityPageFeatureRowElement {
  $: {
    expandButton: CrExpandButtonElement,
  };
}

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
        observer: 'onExpandedChange_',
      },

      icon: {
        type: String,
        reflectToAttribute: true,
      },

      iconVisible: {
        type: Boolean,
        reflectToAttribute: true,
        value: true,
      },

      label: String,

      /* The Preference associated with the feature row. */
      pref: Object,

      subLabel: String,

      numericUncheckedValues: Array,

      numericCheckedValue: Number,

      stateTextMap: Object,

      /**
       * Optional key to override the lookup in stateTextMap.
       * If provided, this is used instead of pref.value.
       */
      currentStateOverrideKey: {
        type: String,
        value: null,
      },

      /* The computed string label for the current pref state. */
      currentStateLabel_: {
        type: String,
        computed:
            'computeCurrentStateLabel_(pref.value, stateTextMap, currentStateKey)',
      },
    };
  }

  declare expanded: boolean;
  declare icon: string;
  declare iconVisible: boolean;
  declare label: string;
  declare pref: chrome.settingsPrivate.PrefObject;
  declare subLabel: string;
  declare numericUncheckedValues: number[];
  declare numericCheckedValue: number;
  declare stateTextMap: Record<string, string>;
  declare currentStateOverrideKey: string|null;
  declare private currentStateLabel_: string;


  private onExpandedChange_() {
    this.dispatchEvent(new CustomEvent('expanded-change', {
      bubbles: true,
      composed: true,
      detail: {value: this.expanded},
    }));

    if (!this.expanded) {
      return;
    }

    // To prevent animation on page load, the transition styling is not applied
    // to the icon until after the row has been expanded.
    // TODO(crbug.com/441316657): Determine the underlying cause and remove
    // this if possible.
    const icon = this.shadowRoot!.querySelector('#icon');
    if (icon) {
      icon.classList.add('enable-transition');
    }
  }

  private onToggleButtonChange_() {
    this.dispatchEvent(new CustomEvent('toggle-button-change', {
      bubbles: true,
      composed: true,
    }));
  }

  private computeCurrentStateLabel_(): string {
    // Determine which key to use for lookup.
    // If currentStateKey is set (not null/empty), use it. Otherwise, use
    // pref.value.
    const key = this.currentStateOverrideKey ? this.currentStateOverrideKey :
                                               this.pref.value;
    if (this.stateTextMap && this.stateTextMap[key] !== undefined) {
      return this.stateTextMap[key];
    }
    // Return an empty string if no mapping is found.
    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'security-page-feature-row': SecurityPageFeatureRowElement;
  }
}

customElements.define(SecurityPageFeatureRowElement.is, SecurityPageFeatureRowElement);
