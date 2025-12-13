// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {BrowserServiceImpl} from './browser_service.js';

import {getCss} from './history_sync_promo.css.js';
import {getHtml} from './history_sync_promo.html.js';

export class HistorySyncPromoElement extends CrLitElement {
  static get is() {
    return 'history-sync-promo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shown_: {type: Boolean},
    };
  }

  protected accessor shown_: boolean = true;

  protected onCloseClick_() {
    this.shown_ = false;
    BrowserServiceImpl.getInstance()
        .handler.incrementHistoryPageHistorySyncPromoShownCount();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-sync-promo': HistorySyncPromoElement;
  }
}

customElements.define(HistorySyncPromoElement.is, HistorySyncPromoElement);
