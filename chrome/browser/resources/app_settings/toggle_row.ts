// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './toggle_row.css.js';
import {getHtml} from './toggle_row.html.js';

export interface ToggleRowElement {
  $: {toggle: CrToggleElement};
}

export class ToggleRowElement extends CrLitElement {
  static get is() {
    return 'app-management-toggle-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      icon: {type: String},
      label: {type: String},
      managed: {
        type: Boolean,
        reflect: true,
      },
      disabled: {
        type: Boolean,
        reflect: true,
      },
      value: {
        type: Boolean,
        reflect: true,
      },
      description: {type: String},
    };
  }

  icon: string = '';
  label: string = '';
  managed: boolean = false;
  disabled: boolean = false;
  value: boolean = false;
  description: string = '';

  override firstUpdated() {
    this.addEventListener('click', this.onClick_);
  }

  isChecked(): boolean {
    return this.$.toggle.checked;
  }

  setToggle(value: boolean) {
    this.$.toggle.checked = value;
  }

  protected isDisabled_(): boolean {
    return this.disabled || this.managed;
  }

  private onClick_(event: Event) {
    event.stopPropagation();
    this.$.toggle.click();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-toggle-row': ToggleRowElement;
  }
}

customElements.define(ToggleRowElement.is, ToggleRowElement);
