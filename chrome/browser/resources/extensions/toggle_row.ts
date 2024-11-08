// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './toggle_row.css.js';
import {getHtml} from './toggle_row.html.js';


/**
 * An extensions-toggle-row provides a way of having a clickable row that can
 * modify a cr-toggle, by leveraging the native <label> functionality. It uses
 * a hidden native <input type="checkbox"> to achieve this.
 */

export interface ExtensionsToggleRowElement {
  $: {
    crToggle: CrToggleElement,
    label: HTMLLabelElement,
    native: HTMLInputElement,
  };
}

export class ExtensionsToggleRowElement extends CrLitElement {
  static get is() {
    return 'extensions-toggle-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      checked: {
        type: Boolean,
        reflect: true,
      },
      disabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  checked: boolean = false;
  disabled: boolean = false;

  /**
   * Exposing the clickable part of extensions-toggle-row for testing
   * purposes.
   */
  getLabel(): HTMLElement {
    return this.$.label;
  }

  protected onNativeClick_(e: Event) {
    // Even though the native checkbox is hidden and can't be actually
    // clicked/tapped by the user, because it resides within the <label> the
    // browser emits an extraneous event when the label is clicked. Stop
    // propagation so that it does not interfere with |onLabelClick_| listener.
    e.stopPropagation();
  }

  private async updateChecked_(value: boolean) {
    this.checked = value;

    // Sync value of native checkbox and cr-toggle and |checked|.
    await this.updateComplete;
    this.fire('change', this.checked);
  }

  /**
   * Fires when the native checkbox changes value. This happens when the user
   * clicks directly on the <label>.
   */
  protected onNativeChange_(e: Event) {
    e.stopPropagation();
    this.updateChecked_(this.$.native.checked);
  }

  protected onCrToggleChange_(e: CustomEvent<boolean>) {
    e.stopPropagation();
    this.updateChecked_(e.detail);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-toggle-row': ExtensionsToggleRowElement;
  }
}

customElements.define(
    ExtensionsToggleRowElement.is, ExtensionsToggleRowElement);
