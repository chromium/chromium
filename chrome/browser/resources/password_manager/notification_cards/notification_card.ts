// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {Page, Router, UrlParam} from '../router.js';
import {BatchUploadPasswordsEntryPoint, SyncBrowserProxyImpl} from '../sync_browser_proxy.js';

import {getCss} from './notification_card.css.js';
import {getHtml} from './notification_card.html.js';
import type {NotificationCard} from './notification_cards_browser_proxy.js';
import {NotificationCardsProxyImpl} from './notification_cards_browser_proxy.js';

// LINT.IfChange(NotificationCardId)
export enum NotificationCardId {
  CHECKUP = 'password_checkup_promo',
  WEB_PASSWORD_MANAGER = 'passwords_on_web_promo',
  SHORTCUT = 'password_shortcut_promo',
  ACCESS_ON_ANY_DEVICE = 'access_on_any_device_promo',
  RELAUNCH_CHROME = 'relaunch_chrome_promo',
  MOVE_PASSWORDS = 'move_passwords_promo',
  // SCREENLOCK_REAUTH = 'screenlock_reauth_promo',  // Obsolete
  PASSKEY_UNLOCK = 'passkey_unlock_promo',
  ACCOUNT_AWARE_PASSWORD_SIGNIN = 'account_aware_password_signin_promo',
}
// LINT.ThenChange(//chrome/browser/ui/webui/password_manager/notification_card.h:NotificationCardId)

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with NotificationCardType in notification_card.h
 */
// LINT.IfChange(NotificationCardMetricId)
enum NotificationCardMetricId {
  CHECKUP = 0,
  UNUSED_WEB_PASSWORD_MANAGER = 1,
  SHORTCUT = 2,
  UNUSED_ACCESS_ON_ANY_DEVICE = 3,
  RELAUNCH_CHROME = 4,
  MOVE_PASSWORDS = 5,
  // SCREENLOCK_REAUTH = 6, Obsolete
  PASSKEY_UNLOCK = 7,
  ACCOUNT_AWARE_PASSWORD_SIGNIN = 8,
  // Must be last.
  COUNT = 9,
}
// LINT.ThenChange(//chrome/browser/ui/webui/password_manager/notification_card.h:NotificationCardType)

function recordNotificationCardAction(card: NotificationCardMetricId) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.PromoCard.ActionButtonClicked', card,
      NotificationCardMetricId.COUNT);
}

export interface NotificationCardElement {
  $: {
    actionButton: CrButtonElement,
    closeButton: CrIconButtonElement,
    description: HTMLElement,
    title: HTMLElement,
  };
}

const isOpenedAsShortcut = window.matchMedia('(display-mode: standalone)');

const NotificationCardElementBase = I18nMixinLit(CrLitElement);

export class NotificationCardElement extends NotificationCardElementBase {
  static get is() {
    return 'notification-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      card: {type: Object},
    };
  }

  accessor card: NotificationCard = {
    id: NotificationCardId.CHECKUP,
    title: '',
    description: '',
    isDismissible: false,
  };

  override connectedCallback() {
    super.connectedCallback();
    // If this is a shortcut promo we should listen to display mode changes to
    // close it automatically when shortcut is installed from another place.
    // Check crbug.com/40075033 for more details when it can happen.
    if (this.card.id === NotificationCardId.SHORTCUT) {
      isOpenedAsShortcut.addEventListener('change', this.close_.bind(this));
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.card.id === NotificationCardId.SHORTCUT) {
      isOpenedAsShortcut.removeEventListener('change', this.close_.bind(this));
    }
  }

  protected getDescription_(): TrustedHTML {
    return sanitizeInnerHtml(this.card.description);
  }

  protected getBannerImageName_(): string {
    if (this.card.id === NotificationCardId.ACCOUNT_AWARE_PASSWORD_SIGNIN) {
      return NotificationCardId.ACCESS_ON_ANY_DEVICE;
    }
    return this.card.id;
  }

  protected onActionButtonClick_() {
    switch (this.card.id) {
      case NotificationCardId.CHECKUP:
        const params = new URLSearchParams();
        params.set(UrlParam.START_CHECK, 'true');
        Router.getInstance().navigateTo(Page.CHECKUP, null, params);
        recordNotificationCardAction(NotificationCardMetricId.CHECKUP);
        break;
      case NotificationCardId.SHORTCUT:
        PasswordManagerImpl.getInstance().showAddShortcutDialog();
        recordNotificationCardAction(NotificationCardMetricId.SHORTCUT);
        break;
      case NotificationCardId.RELAUNCH_CHROME:
        chrome.send('restartBrowser');
        recordNotificationCardAction(NotificationCardMetricId.RELAUNCH_CHROME);
        break;
      case NotificationCardId.MOVE_PASSWORDS:
        SyncBrowserProxyImpl.getInstance().openBatchUpload(
            BatchUploadPasswordsEntryPoint.PROMO_CARD);
        recordNotificationCardAction(NotificationCardMetricId.MOVE_PASSWORDS);
        break;
      case NotificationCardId.PASSKEY_UNLOCK:
        SyncBrowserProxyImpl.getInstance().startPasskeyUnlockFlow();
        recordNotificationCardAction(NotificationCardMetricId.PASSKEY_UNLOCK);
        break;
      case NotificationCardId.ACCOUNT_AWARE_PASSWORD_SIGNIN:
        NotificationCardsProxyImpl.getInstance().signInFromNotificationCard();
        recordNotificationCardAction(
            NotificationCardMetricId.ACCOUNT_AWARE_PASSWORD_SIGNIN);
        break;
      default:
        assertNotReached();
    }
    this.close_();
  }

  protected onCloseClick_() {
    NotificationCardsProxyImpl.getInstance().recordNotificationDismissed(
        this.card.id);
    this.close_();
  }

  private close_() {
    this.fire('card-closed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'notification-card': NotificationCardElement;
  }
}

customElements.define(NotificationCardElement.is, NotificationCardElement);
