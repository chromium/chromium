// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './spinner_page.css.js';
import {getHtml} from './spinner_page.html.js';
import {SupportToolPageMixinLit} from './support_tool_page_mixin_lit.js';

export interface SpinnerPageElement {
  $: {
    cancelButton: CrButtonElement,
  };
}

const SpinnerPageElementBase = SupportToolPageMixinLit(CrLitElement);

export class SpinnerPageElement extends SpinnerPageElementBase {
  static get is() {
    return 'spinner-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageTitle: {type: String},
    };
  }

  protected accessor pageTitle: string = '';
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected onCancelClick_() {
    this.browserProxy_.cancelDataCollection();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'spinner-page': SpinnerPageElement;
  }
}

customElements.define(SpinnerPageElement.is, SpinnerPageElement);
