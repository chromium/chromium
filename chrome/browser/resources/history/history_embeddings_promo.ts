// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './history_embeddings_promo.html.js';

export interface HistoryEmbeddingsPromoElement {
  $: {
    close: HTMLElement,
    promo: HTMLElement,
  };
}

export class HistoryEmbeddingsPromoElement extends PolymerElement {
  static get is() {
    return 'history-embeddings-promo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shown_: Boolean,
    };
  }

  private shown_: boolean = true;

  private onCloseClick_() {
    this.shown_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-embeddings-promo': HistoryEmbeddingsPromoElement;
  }
}

customElements.define(
    HistoryEmbeddingsPromoElement.is, HistoryEmbeddingsPromoElement);
