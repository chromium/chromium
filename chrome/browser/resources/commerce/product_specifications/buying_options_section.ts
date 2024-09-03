// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './buying_options_section.css.js';
import {getHtml} from './buying_options_section.html.js';

export interface BuyingOptionsLink {
  jackpotUrl: string;
}

export interface BuyingOptionsSectionElement {
  $: {
    link: HTMLElement,
  };
}

export class BuyingOptionsSectionElement extends CrLitElement {
  static get is() {
    return 'buying-options-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      jackpotUrl: {type: String},
    };
  }

  jackpotUrl: string = '';

  protected openJackpotUrl_() {
    OpenWindowProxyImpl.getInstance().openUrl(this.jackpotUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'buying-options-section': BuyingOptionsSectionElement;
  }
}

customElements.define(
    BuyingOptionsSectionElement.is, BuyingOptionsSectionElement);
