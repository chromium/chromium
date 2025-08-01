// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A component for displaying a single NTP Promo.
 */
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './ntp_promo_icons.html.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {NtpPromoClientCallbackRouter, NtpPromoHandlerInterface, Promo} from '../ntp_promo.mojom-webui.js';

import {getCss} from './ntp_promo.css.js';
import {NtpPromoProxyImpl} from './ntp_promo_proxy.js';
import {getHtml} from './ntp_single_promo.html.js';

export interface NtpSinglePromoElement {
  $: {
    actionButton: CrIconButtonElement,
    bodyIcon: HTMLElement,
    bodyText: HTMLElement,
  };
}

export class NtpSinglePromoElement extends CrLitElement {
  static get is() {
    return 'ntp-single-promo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      promoId: {type: String, reflect: true, useDefault: true},
    };
  }

  accessor promoId: string = '';

  protected bodyIconName_: string = '';
  protected bodyText_: string = '';
  protected actionButtonText_: string = '';

  private handler_: NtpPromoHandlerInterface;
  private callbackRouter_: NtpPromoClientCallbackRouter;
  private listenerIds_: number[] = [];
  private notifiedShown_: boolean = false;

  constructor() {
    super();
    this.handler_ = NtpPromoProxyImpl.getInstance().getHandler();
    this.callbackRouter_ = NtpPromoProxyImpl.getInstance().getCallbackRouter();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_.push(this.callbackRouter_.setPromos.addListener(
        this.onSetPromos.bind(this))),
        this.handler_.requestPromos();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    for (const listenerId of this.listenerIds_) {
      this.callbackRouter_.removeListener(listenerId);
    }
    this.listenerIds_ = [];
  }

  // Public for testing purposes only.
  onSetPromos(eligible: Promo[]) {
    if (eligible.length > 0) {
      const promo: Promo = eligible[0]!;
      this.bodyIconName_ = promo.iconName;
      this.bodyText_ = promo.bodyText;
      this.actionButtonText_ = promo.buttonText;
      this.promoId = promo.id;
      this.style.display = 'block';
    } else {
      this.promoId = '';
      this.bodyIconName_ = '';
      this.bodyText_ = '';
      this.actionButtonText_ = '';
      this.style.display = 'none';
    }
    if (!this.notifiedShown_) {
      this.notifiedShown_ = true;
      const shown: string[] = this.promoId ? [this.promoId] : [];
      this.handler_.onPromosShown(shown, []);
    }
  }

  protected onButtonClick_() {
    assert(this.promoId, 'Button should not be able to display if no promoId.');
    this.handler_.onPromoClicked(this.promoId);
  }
}

customElements.define(NtpSinglePromoElement.is, NtpSinglePromoElement);

declare global {
  interface HTMLElementTagNameMap {
    'ntp-single-promo': NtpSinglePromoElement;
  }
}
