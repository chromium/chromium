// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BatchUploadPromoData, BrowserProxy} from './browser_proxy.js';
import {getCss} from './promo_card.css.js';
import {getHtml} from './promo_card.html.js';

export interface PromoCardElement {
  $: {
    actionButton: CrButtonElement,
    closeButton: CrIconButtonElement,
    description: HTMLElement,
    title: HTMLElement,
  };
}

const PromoCardElementBase = WebUiListenerMixinLit(CrLitElement);

export class PromoCardElement extends PromoCardElementBase {
  static get is() {
    return 'promo-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      batchUploadPromoData_: {type: Object},
    };
  }

  protected accessor batchUploadPromoData_: BatchUploadPromoData = {
    canShow: false,
    promoSubtitle: '',
  };

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getBatchUploadPromoInfo().then(
        this.updateBatchUploadPromoData_.bind(this));
    this.addWebUiListener(
        'batch-upload-promo-info-updated',
        this.updateBatchUploadPromoData_.bind(this));
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  private updateBatchUploadPromoData_(promoData: BatchUploadPromoData) {
    this.batchUploadPromoData_ = promoData;
    this.propagateShouldShowPromo_(this.batchUploadPromoData_.canShow);
  }

  protected onSaveToAccountClick_() {
    this.browserProxy_.onBatchUploadPromoClicked();
  }

  protected onCloseClick_() {
    this.browserProxy_.onBatchUploadPromoDismissed();
    // Allows to close the promo right away instead of waiting for the
    // notification from the browser.
    this.propagateShouldShowPromo_(false);
  }

  // Trigger update on the list.
  private propagateShouldShowPromo_(shouldShow: boolean) {
    this.fire('on-should-show-promo-card', {shouldShowPromoCard: shouldShow});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'promo-card': PromoCardElement;
  }
}

customElements.define(PromoCardElement.is, PromoCardElement);
