// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './toggle_row.html.js';


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

export class ExtensionsToggleRowElement extends PolymerElement {
  static get is() {
    return 'extensions-toggle-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked: Boolean,

      disabled: Boolean,
    };
  }

  checked: boolean;
  disabled: boolean;

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /**
   * Exposing the clickable part of extensions-toggle-row for testing
   * purposes.
   */
  getLabel(): HTMLElement {
    return this.$.label;
  }

  private onNativeClick_(e: Event) {
    // Even though the native checkbox is hidden and can't be actually
    // cilcked/tapped by the user, because it resides within the <label> the
    // browser emits an extraneous event when the label is clicked. Stop
    // propagation so that it does not interfere with |onLabelClick_| listener.
    e.stopPropagation();
  }

  /**
   * Fires when the native checkbox changes value. This happens when the user
   * clicks directly on the <label>.
   */
  private onNativeChange_(e: Event) {
    e.stopPropagation();

    // Sync value of native checkbox and cr-toggle and |checked|.
    this.$.crToggle.checked = this.$.native.checked;
    this.checked = this.$.native.checked;

    this.fire_('change', this.checked);
  }

  private onCrToggleChange_(e: CustomEvent<boolean>) {
    e.stopPropagation();

    // Sync value of native checkbox and cr-toggle.
    this.$.native.checked = e.detail;

    this.fire_('change', this.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-toggle-row': ExtensionsToggleRowElement;
  }
}

customElements.define(
    ExtensionsToggleRowElement.is, ExtensionsToggleRowElement);
