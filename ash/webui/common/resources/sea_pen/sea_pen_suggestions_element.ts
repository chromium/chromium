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

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SEA_PEN_SUGGESTIONS} from './constants.js';
import {getTemplate} from './sea_pen_suggestions_element.html.js';
import {shuffle} from './sea_pen_utils.js';

const SeaPenSuggestionsElementBase = I18nMixin(PolymerElement);

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

export class SeaPenSuggestionsElement extends SeaPenSuggestionsElementBase {
  static get is() {
    return 'sea-pen-suggestions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      suggestions: {
        type: Array,
        value: SEA_PEN_SUGGESTIONS,
      },
    };
  }

  private suggestions: string[];

  private onClickSuggestion_(event: Event) {
    const target = event.currentTarget as HTMLElement;
    const suggestion = target.textContent?.trim();
    assert(suggestion);
    this.dispatchEvent(new SeaPenSuggestionSelectedEvent(suggestion));
  }

  private onShuffleClicked_() {
    this.suggestions = shuffle(this.suggestions);
  }
}

customElements.define(SeaPenSuggestionsElement.is, SeaPenSuggestionsElement);
