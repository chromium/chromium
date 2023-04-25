// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {Page, Router, UrlParam} from '../router.js';

import {getTemplate} from './promo_card.html.js';
import {PromoCard, PromoCardsProxyImpl} from './promo_cards_browser_proxy.js';

// WARNING: Keep synced with
// chrome/browser/ui/webui/password_manager/promo_cards_handler.cc.
export enum PromoCardId {
  CHECKUP = 'password_checkup_promo',
  WEB_PASSWORD_MANAGER = 'passwords_on_web_promo',
  SHORTCUT = 'password_shortcut_promo',
  ACCESS_ON_ANY_DEVICE = 'access_on_any_device_promo',
}

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PromoCardType in promo_card.cc
 */
enum PromoCardMetricId {
  CHECKUP = 0,
  UNUSED_WEB_PASSWORD_MANAGER = 1,
  SHORTCUT = 2,
  UNUSED_ACCESS_ON_ANY_DEVICE = 3,
  // Must be last.
  COUNT = 4,
}

function recordPromoCardAction(card: PromoCardMetricId) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.PromoCard.ActionButtonClicked', card,
      PromoCardMetricId.COUNT);
}

export interface PromoCardElement {
  $: {
    actionButton: CrButtonElement,
    closeButton: CrIconButtonElement,
    description: HTMLElement,
    title: HTMLElement,
  };
}

export class PromoCardElement extends PolymerElement {
  static get is() {
    return 'promo-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      promoCard: Object,
    };
  }

  promoCard: PromoCard;

  private getDescription_(): TrustedHTML {
    return sanitizeInnerHtml(this.promoCard.description);
  }

  private onActionButtonClick_() {
    switch (this.promoCard.id) {
      case PromoCardId.CHECKUP:
        const params = new URLSearchParams();
        params.set(UrlParam.START_CHECK, 'true');
        Router.getInstance().navigateTo(Page.CHECKUP, null, params);
        recordPromoCardAction(PromoCardMetricId.CHECKUP);
        break;
      case PromoCardId.SHORTCUT:
        PasswordManagerImpl.getInstance().showAddShortcutDialog();
        recordPromoCardAction(PromoCardMetricId.SHORTCUT);
        break;
      default:
        assertNotReached();
    }
    this.close_();
  }

  private onCloseClick_() {
    PromoCardsProxyImpl.getInstance().recordPromoDismissed(this.promoCard.id);
    this.close_();
  }

  private close_() {
    this.dispatchEvent(
        new CustomEvent('promo-closed', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'promo-card': PromoCardElement;
  }
}

customElements.define(PromoCardElement.is, PromoCardElement);
