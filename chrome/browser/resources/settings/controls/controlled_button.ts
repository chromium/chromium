// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {CrPolicyPrefMixinLit} from '/shared/settings/controls/cr_policy_pref_mixin_lit.js';

import {getCss} from './controlled_button.css.js';
import {getHtml} from './controlled_button.html.js';
import {PrefKeyObserverMixinLit} from './pref_key_observer_mixin_lit.js';

export interface ControlledButtonElement {
  $: {
    button: CrButtonElement,
  };
}

const ControlledButtonElementBase =
    CrPolicyPrefMixinLit(PrefKeyObserverMixinLit(CrLitElement));

export class ControlledButtonElement extends ControlledButtonElementBase {
  static get is() {
    return 'controlled-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      endJustified: {
        type: Boolean,
        reflect: true,
      },

      label: {type: String},

      disabled: {
        type: Boolean,
        reflect: true,
      },

      actionClass_: {type: String},

      enforced_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor endJustified: boolean = false;
  accessor label: string = '';
  accessor disabled: boolean = false;
  protected accessor actionClass_: string = '';
  protected accessor enforced_: boolean = false;

  override connectedCallback() {
    if (this.classList.contains('action-button')) {
      this.actionClass_ = 'action-button';
    }

    super.connectedCallback();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('pref')) {
      this.enforced_ = this.isPrefEnforced();
    }
  }

  /** Focus on the inner cr-button. */
  override focus() {
    this.$.button.focus();
  }

  protected onIndicatorClick_(e: Event) {
    // Disallow <controlled-button on-click="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  }

  protected buttonEnabled_(): boolean {
    return !this.enforced_ && !this.disabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'controlled-button': ControlledButtonElement;
  }
}

customElements.define(ControlledButtonElement.is, ControlledButtonElement);
