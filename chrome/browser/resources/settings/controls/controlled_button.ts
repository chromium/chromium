// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrPolicyPrefMixin} from '/shared/settings/controls/cr_policy_pref_mixin.js';
import {PrefControlMixin} from '/shared/settings/controls/pref_control_mixin.js';

import {getTemplate} from './controlled_button.html.js';

const ControlledButtonElementBase =
    CrPolicyPrefMixin(PrefControlMixin(PolymerElement));

export class ControlledButtonElement extends ControlledButtonElementBase {
  static get is() {
    return 'controlled-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      endJustified: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      label: String,

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      actionClass_: {type: String, value: ''},

      enforced_: {
        type: Boolean,
        computed: 'isPrefEnforced(pref.*)',
        reflectToAttribute: true,
      },
    };
  }

  endJustified: boolean;
  label: string;
  disabled: boolean;
  private actionClass_: string;
  private enforced_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    if (this.classList.contains('action-button')) {
      this.actionClass_ = 'action-button';
    }
  }

  /** Focus on the inner cr-button. */
  override focus() {
    this.shadowRoot!.querySelector('cr-button')!.focus();
  }

  private onIndicatorClick_(e: Event) {
    // Disallow <controlled-button on-click="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  }

  private buttonEnabled_(enforced: boolean, disabled: boolean): boolean {
    return !enforced && !disabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'controlled-button': ControlledButtonElement;
  }
}

customElements.define(ControlledButtonElement.is, ControlledButtonElement);
