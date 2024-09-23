// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {Page, Router, UrlParam} from '../router.js';

import {getTemplate} from './promo_card.html.js';
import type {PromoCard} from './promo_cards_browser_proxy.js';
import {PromoCardsProxyImpl} from './promo_cards_browser_proxy.js';

// WARNING: Keep synced with
// chrome/browser/ui/webui/password_manager/promo_cards_handler.cc.
export enum PromoCardId {
  CHECKUP = 'password_checkup_promo',
  WEB_PASSWORD_MANAGER = 'passwords_on_web_promo',
  SHORTCUT = 'password_shortcut_promo',
  ACCESS_ON_ANY_DEVICE = 'access_on_any_device_promo',
  RELAUNCH_CHROME = 'relaunch_chrome_promo',
  MOVE_PASSWORDS = 'move_passwords_promo',
  SCREENLOCK_REAUTH = 'screenlock_reauth_promo',
}

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PromoCardType in promo_card.h
 */
enum PromoCardMetricId {
  CHECKUP = 0,
  UNUSED_WEB_PASSWORD_MANAGER = 1,
  SHORTCUT = 2,
  UNUSED_ACCESS_ON_ANY_DEVICE = 3,
  RELAUNCH_CHROME = 4,
  MOVE_PASSWORDS = 5,
  SCREENLOCK_REAUTH = 6,
  // Must be last.
  COUNT = 7,
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

const isOpenedAsShortcut = window.matchMedia('(display-mode: standalone)');

const PromoCardElementBase = I18nMixin(PolymerElement);

export class PromoCardElement extends PromoCardElementBase {
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

  override connectedCallback() {
    super.connectedCallback();
    // If this is a shortcut promo we should listen to display mode changes to
    // close it automatically when shortcut is installed from another place.
    // Check crbug.com/1493264 for more details when it can happen.
    if (this.promoCard.id === PromoCardId.SHORTCUT) {
      isOpenedAsShortcut.addEventListener('change', this.close_.bind(this));
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.promoCard.id === PromoCardId.SHORTCUT) {
      isOpenedAsShortcut.removeEventListener('change', this.close_.bind(this));
    }
  }

  private getDescription_(): TrustedHTML {
    return sanitizeInnerHtml(this.promoCard.description);
  }

  private async onActionButtonClick_() {
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
      case PromoCardId.RELAUNCH_CHROME:
        chrome.send('restartBrowser');
        recordPromoCardAction(PromoCardMetricId.RELAUNCH_CHROME);
        break;
      case PromoCardId.MOVE_PASSWORDS:
        this.dispatchEvent(new CustomEvent(
            'move-passwords-clicked', {bubbles: true, composed: true}));
        recordPromoCardAction(PromoCardMetricId.MOVE_PASSWORDS);
        return;
      case PromoCardId.SCREENLOCK_REAUTH:
        recordPromoCardAction(PromoCardMetricId.SCREENLOCK_REAUTH);
        await PasswordManagerImpl.getInstance()
            .switchBiometricAuthBeforeFillingState()
            .then(result => {
              if (result) {
                this.dispatchEvent(new CustomEvent(
                    'biometric-auth-before-filling-enabled',
                    {bubbles: true, composed: true}));
              }
            });
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
