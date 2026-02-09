// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ManagementBrowserProxy} from './management_browser_proxy.js';
import {ManagementBrowserProxyImpl} from './management_browser_proxy.js';
import {getCss} from './promotion_banner.css.js';
import {getHtml} from './promotion_banner.html.js';

export class PromotionBannerElement extends CrLitElement {
  static get is() {
    return 'promotion-banner';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private browserProxy_: ManagementBrowserProxy =
      ManagementBrowserProxyImpl.getInstance();

  protected onDismissPromotionClick_() {
    this.fire('promotion-dismissed');
    this.browserProxy_.setBannerDismissed();
  }

  protected onPromotionRedirectClick_() {
    window.open(
        'https://admin.google.com/ac/chrome/guides/?ref=browser&utm_source=chrome_policy_cec',
        '_blank');
    this.browserProxy_.recordBannerRedirected();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'promotion-banner': PromotionBannerElement;
  }
}

customElements.define(PromotionBannerElement.is, PromotionBannerElement);
