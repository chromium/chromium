// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-card' shows a paper material themed card with an optional
 * header.
 *
 * Example:
 *    <settings-card header-text="[[headerText]]">
 *      <!-- Insert card content here -->
 *    </settings-card>
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_card.html.js';

export class SettingsCardElement extends PolymerElement {
  static get is() {
    return 'settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Header text for the card. Initialize so we can use the
       * getHeaderTextAriaHidden_ method for accessibility.
       */
      headerText: {
        type: String,
        value: '',
      },
    };
  }

  headerText: string;

  /**
   * Get the aria-hidden value for the header text.
   * @return A return value of false will not add the aria-hidden attribute,
   *    while a value of 'true' will add aria-hidden="true" per the ARIA specs.
   */
  private getHeaderTextAriaHidden_(): string|boolean {
    return this.headerText ? false : 'true';
  }

  override focus(): void {
    this.shadowRoot!.getElementById('headerText')!.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsCardElement.is]: SettingsCardElement;
  }
}

customElements.define(SettingsCardElement.is, SettingsCardElement);
