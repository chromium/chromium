// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-offers-and-loyalty-cards-page' is a subpage of the
 * "Your saved info" section. It will allow users to manage their Google Wallet
 * offers and loyalty cards and control whether they are used for autofill.
 */
import '../../settings_page/settings_subpage.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';

import {getCss} from './offers_and_loyalty_cards_page.css.js';
import {getHtml} from './offers_and_loyalty_cards_page.html.js';

const SettingsOffersAndLoyaltyCardsPageElementBase =
    SettingsViewMixinLit(CrLitElement);

export class SettingsOffersAndLoyaltyCardsPageElement extends
    SettingsOffersAndLoyaltyCardsPageElementBase {
  static get is() {
    return 'settings-offers-and-loyalty-cards-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-offers-and-loyalty-cards-page':
        SettingsOffersAndLoyaltyCardsPageElement;
  }
}

customElements.define(
    SettingsOffersAndLoyaltyCardsPageElement.is,
    SettingsOffersAndLoyaltyCardsPageElement);
