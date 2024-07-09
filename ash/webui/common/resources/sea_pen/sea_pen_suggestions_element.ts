// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays all the suggestions to fill in
 * the template placeholder.
 */

import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';

import {assert} from 'chrome://resources/js/assert.js';

import {SEA_PEN_SUGGESTIONS} from './constants.js';
import {SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {getTemplate} from './sea_pen_suggestions_element.html.js';
import {isArrayEqual, shuffle} from './sea_pen_utils.js';

const seaPenSuggestionSelectedEvent = 'sea-pen-suggestion-selected';

export class SeaPenSuggestionSelectedEvent extends CustomEvent<string> {
  constructor(suggestion: string) {
    super(seaPenSuggestionSelectedEvent, {
      bubbles: true,
      composed: true,
      detail: suggestion,
    });
  }
}

declare global {
  interface HTMLElementEventMap {
    [seaPenSuggestionSelectedEvent]: SeaPenSuggestionSelectedEvent;
  }
}

export class SeaPenSuggestionsElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-suggestions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      suggestions_: Array,

      hiddenSuggestions_: Object,

      thumbnails_: {
        type: Object,
        observer: 'resetSuggestions_',
      },
    };
  }

  private suggestions_: string[];
  private hiddenSuggestions_: Set<string>;
  private thumbnails_: SeaPenThumbnail[]|null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.watch<SeaPenSuggestionsElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
    this.hiddenSuggestions_ = new Set();
    this.suggestions_ = [...SEA_PEN_SUGGESTIONS];
  }

  private resetSuggestions_() {
    this.hiddenSuggestions_ = new Set();
    this.onShuffleClicked_();
  }

  private onClickSuggestion_(event: Event&{model: {index: number}}) {
    const target = event.currentTarget as HTMLElement;
    const suggestion = target.textContent?.trim();
    assert(suggestion);
    this.dispatchEvent(new SeaPenSuggestionSelectedEvent(suggestion));
    this.splice('suggestions_', event.model.index, 1);
    this.hiddenSuggestions_.add(suggestion);
  }

  private onShuffleClicked_() {
    // Run shuffle (5 times at most) until the shuffled suggestions are
    // different from current; which is highly likely to happen the first time.
    for (let i = 0; i < 5; i++) {
      // If there are more than three suggestions, filter the hidden suggestions
      // out. Otherwise, use the full list of suggestions.
      const filteredSuggestions =
          SEA_PEN_SUGGESTIONS.length - this.hiddenSuggestions_.size > 3 ?
          SEA_PEN_SUGGESTIONS.filter(s => !this.hiddenSuggestions_.has(s)) :
          SEA_PEN_SUGGESTIONS;
      const newSuggestions = shuffle(filteredSuggestions);
      if (!isArrayEqual(newSuggestions, this.suggestions_)) {
        this.suggestions_ = newSuggestions;
        break;
      }
    }
    this.hiddenSuggestions_ = new Set();
  }
}

customElements.define(SeaPenSuggestionsElement.is, SeaPenSuggestionsElement);
