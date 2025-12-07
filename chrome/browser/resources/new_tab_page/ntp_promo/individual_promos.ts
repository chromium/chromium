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

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {NtpPromoClientCallbackRouter, NtpPromoHandlerInterface, Promo} from '../ntp_promo.mojom-webui.js';

import {getCss} from './individual_promos.css.js';
import {getHtml} from './individual_promos.html.js';
import {NtpPromoProxyImpl} from './ntp_promo_proxy.js';

export interface IndividualPromosElement {
  $: {promos: HTMLElement};
}

export class IndividualPromosElement extends CrLitElement {
  static get is() {
    return 'individual-promos';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      eligiblePromos_: {type: Array},
      maxPromos: {type: Number, attribute: true, useDefault: true},
    };
  }


  accessor eligiblePromos_: Promo[] = [];
  accessor maxPromos: number = 0;

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
        this.onSetPromos.bind(this)));
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
    this.eligiblePromos_ =
        eligible.slice(0, Math.min(this.maxPromos, eligible.length));

    if (this.eligiblePromos_.length > 0) {
      this.style.display = 'block';
    } else {
      this.style.display = 'none';
    }
    if (!this.notifiedShown_) {
      this.notifiedShown_ = true;
      const shown = this.eligiblePromos_.map(p => p.id);
      this.handler_.onPromosShown(shown, []);
    }
  }

  protected onClick_(promoId: string) {
    assert(promoId, 'Button should not be able to display if no promoId.');
    this.handler_.onPromoClicked(promoId);
  }

  protected getBodyTextCssClass_(): string {
    return this.eligiblePromos_.length > 1 ? 'multiplePromos' : 'singlePromo';
  }
}

customElements.define(IndividualPromosElement.is, IndividualPromosElement);

declare global {
  interface HTMLElementTagNameMap {
    'individual-promos': IndividualPromosElement;
  }
}
