// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
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

  private blocklistedPromo_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();
    NewTabPageProxy.getInstance().handler.getMobilePromoQrCode().then(
        ({qrCode}) => {
          this.qrCode = qrCode;
        });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'keydown', this.onWindowKeydown_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected onDismissButtonClick_() {
    this.blocklistedPromo_ = true;
    NewTabPageProxy.getInstance().handler.onDismissMobilePromo();
    this.$.promoContainer.hidden = true;
    this.$.dismissPromoButtonToast.show();
  }

  protected onUndoDismissPromoButtonClick_() {
    this.blocklistedPromo_ = false;
    NewTabPageProxy.getInstance().handler.onUndoDismissMobilePromo();
    this.$.promoContainer.hidden = false;
    this.$.dismissPromoButtonToast.hide();
  }

  // Allow users to undo the dismissal of the mobile promo using Ctrl+Z (or
  // Cmd+Z on macOS).
  private onWindowKeydown_(e: KeyboardEvent) {
    if (!this.blocklistedPromo_) {
      return;
    }
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.key === 'z') {
      this.onUndoDismissPromoButtonClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-mobile-promo': MobilePromoElement;
  }
}

customElements.define(MobilePromoElement.is, MobilePromoElement);
