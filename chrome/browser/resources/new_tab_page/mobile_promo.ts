// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './mobile_promo.css.js';
import {getHtml} from './mobile_promo.html.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';

export interface MobilePromoElement {
  $: {
    dismissPromoButtonToast: CrToastElement,
    titleAndDismissContainer: HTMLElement,
    promoContainer: HTMLElement,
    undoDismissPromoButton: HTMLElement,
  };
}

/**
 * @fileoverview This file provides a custom element for displaying a mobile
 * promo in the NTP's middle slot.
 */
export class MobilePromoElement extends CrLitElement {
  static get is() {
    return 'ntp-mobile-promo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      qrCode: {
        type: String,
        notify: true,
      },
    };
  }

  protected qrCode: string;

  constructor() {
    super();
    NewTabPageProxy.getInstance().handler.getMobilePromoQrCode().then(
        ({qrCode}) => {
          this.qrCode = qrCode;
        });
  }

  protected onDismissButtonClick_() {
    this.$.promoContainer.hidden = true;
    this.$.dismissPromoButtonToast.show();
    // TODO(crbub.com/372306118): Handle dismiss in C++.
  }

  protected onUndoDismissPromoButtonClick_() {
    this.$.promoContainer.hidden = false;
    this.$.dismissPromoButtonToast.hide();
    // TODO(crbub.com/372306118): Handle dismiss in C++.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-mobile-promo': MobilePromoElement;
  }
}

customElements.define(MobilePromoElement.is, MobilePromoElement);
