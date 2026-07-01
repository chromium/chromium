// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './history_cross_device_signin_promo.css.js';
import {getHtml} from './history_cross_device_signin_promo.html.js';
import {HistoryCrossDeviceSigninPromoBrowserProxy} from './history_cross_device_signin_promo_browser_proxy.js';

export interface HistoryCrossDeviceSigninPromoElement {
  $: {
    actionButton: CrButtonElement,
    close: CrIconButtonElement,
  };
}

const HistoryCrossDeviceSigninPromoElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class HistoryCrossDeviceSigninPromoElement extends
    HistoryCrossDeviceSigninPromoElementBase {
  static get is() {
    return 'history-cross-device-signin-promo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.handler_.shouldShowPromoCard().then(
        ({shouldShow}: {shouldShow: boolean}) => {
          this.propagateShouldShowPromo_(shouldShow);
          if (shouldShow) {
            this.handler_.onPromoCardShown();
          }
        });
  }

  private handler_ =
      HistoryCrossDeviceSigninPromoBrowserProxy.getInstance().handler;

  protected onActionButtonClick_() {
    this.$.actionButton.disabled = true;
    this.handler_.onPromoCardActionClicked().then(() => {
      this.propagateShouldShowPromo_(false);
    });
  }

  protected onCloseClick_() {
    this.handler_.onPromoCardDismissed();
    this.propagateShouldShowPromo_(false);
  }

  private propagateShouldShowPromo_(shouldShow: boolean) {
    this.fire('should-show-history-cross-device-signin-promo', {shouldShow});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-cross-device-signin-promo': HistoryCrossDeviceSigninPromoElement;
  }
}

customElements.define(
    HistoryCrossDeviceSigninPromoElement.is,
    HistoryCrossDeviceSigninPromoElement);
