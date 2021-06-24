// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'credit-card-list-entry' is a credit card row to be shown in
 * the settings page.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../i18n_setup.js';
import '../settings_shared_css.js';
import './passwords_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsCreditCardListEntryElement extends PolymerElement {
  static get is() {
    return 'settings-credit-card-list-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * A saved credit card.
       * @type {!chrome.autofillPrivate.CreditCardEntry}
       */
      creditCard: Object,
    };
  }

  /**
   * Opens the credit card action menu.
   * @private
   */
  onDotsMenuClick_() {
    this.dispatchEvent(new CustomEvent('dots-card-menu-click', {
      bubbles: true,
      composed: true,
      detail: {
        creditCard: this.creditCard,
        anchorElement: this.shadowRoot.querySelector('#creditCardMenu'),
      }
    }));
  }

  /** @private */
  onRemoteEditClick_() {
    this.dispatchEvent(new CustomEvent('remote-card-menu-click', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * The 3-dot menu should not be shown if the card is entirely remote.
   * @return {boolean}
   * @private
   */
  showDots_() {
    return !!(
        this.creditCard.metadata.isLocal || this.creditCard.metadata.isCached);
  }
}

customElements.define(
    SettingsCreditCardListEntryElement.is, SettingsCreditCardListEntryElement);
