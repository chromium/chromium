// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './banner_promo.css.js';
import {getHtml} from './banner_promo.html.js';

export class BannerPromoElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-banner-promo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      acceptButtonText: {type: String},
      dismissButtonText: {type: String},
    };
  }

  accessor acceptButtonText: string = '';
  accessor dismissButtonText: string = '';

  protected onNotNowClick_() {
    this.fire('dismiss');
  }

  protected onTurnOnClick_() {
    this.fire('accept');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-banner-promo': BannerPromoElement;
  }
}

customElements.define(BannerPromoElement.is, BannerPromoElement);
