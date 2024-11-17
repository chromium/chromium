// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './history_embeddings_promo.html.js';

// Key used in localStorage to determine if this promo has been shown. Any value
// is considered truthy.
export const HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY: string =
    'history-embeddings-promo';

// Key used in localStorage to determine if this promo has been shown for
// history embeddings with answerer enabled. Any value is considered truthy.
export const HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY: string =
    'history-embeddings-answers-promo';

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
      isAnswersEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableHistoryEmbeddingsAnswers'),
      },

      shown_: {
        type: Boolean,
        value: () => {
          const key =
              loadTimeData.getBoolean('enableHistoryEmbeddingsAnswers') ?
              HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY :
              HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY;
          return !(window.localStorage.getItem(key));
        },
      },
    };
  }

  private shown_: boolean;

  private onCloseClick_() {
    const key = loadTimeData.getBoolean('enableHistoryEmbeddingsAnswers') ?
        HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY :
        HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY;
    window.localStorage.setItem(key, true.toString());
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
