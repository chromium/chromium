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
import {BatchUploadPasswordsEntryPoint, SyncBrowserProxyImpl} from '../sync_browser_proxy.js';

import {getTemplate} from './notification_card.html.js';
import type {NotificationCard} from './notification_cards_browser_proxy.js';
import {NotificationCardsProxyImpl} from './notification_cards_browser_proxy.js';

// WARNING: Keep synced with
// chrome/browser/ui/webui/password_manager/notification_cards_handler.cc.
export enum NotificationCardId {
  CHECKUP = 'password_checkup_promo',
  WEB_PASSWORD_MANAGER = 'passwords_on_web_promo',
  SHORTCUT = 'password_shortcut_promo',
  ACCESS_ON_ANY_DEVICE = 'access_on_any_device_promo',
  RELAUNCH_CHROME = 'relaunch_chrome_promo',
  MOVE_PASSWORDS = 'move_passwords_promo',
  SCREENLOCK_REAUTH = 'screenlock_reauth_promo',  // Obsolete
}

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
  // Must be last.
  COUNT = 7,
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

const NotificationCardElementBase = I18nMixin(PolymerElement);

export class NotificationCardElement extends NotificationCardElementBase {
  static get is() {
    return 'notification-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      card: Object,
    };
  }

  declare card: NotificationCard;

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

  private getDescription_(): TrustedHTML {
    return sanitizeInnerHtml(this.card.description);
  }

  private onActionButtonClick_() {
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
      default:
        assertNotReached();
    }
    this.close_();
  }

  private onCloseClick_() {
    NotificationCardsProxyImpl.getInstance().recordNotificationDismissed(
        this.card.id);
    this.close_();
  }

  private close_() {
    this.dispatchEvent(
        new CustomEvent('card-closed', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'notification-card': NotificationCardElement;
  }
}

customElements.define(NotificationCardElement.is, NotificationCardElement);
