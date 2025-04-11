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
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import type {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import type {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {afterNextRender, Debouncer, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SEA_PEN_SUGGESTIONS} from './constants.js';
import {logSuggestionClicked, logSuggestionShuffleClicked} from './sea_pen_metrics_logger.js';
import {getTemplate} from './sea_pen_suggestions_element.html.js';
import {IronAnnounceEvent, isArrayEqual, isNonEmptyArray, shuffle} from './sea_pen_utils.js';

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

export interface SeaPenSuggestionsElement {
  $: {
    keys: IronA11yKeysElement,
    suggestionSelector: IronSelectorElement,
    shuffle: CrButtonElement,
  };
}

const SeaPenSuggestionsElementBase = I18nMixin(PolymerElement);

export class SeaPenSuggestionsElement extends SeaPenSuggestionsElementBase {
  static get is() {
    return 'sea-pen-suggestions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      suggestions_: {
        type: Array,
        observer: 'onSuggestionsChanged_',
      },

      // An array to store only the visible suggestions to select by filling the
      // items in `suggestions_` to the `suggestionSelector` container until no
      // space (width) left.
      selectableSuggestions_: Array,

      hiddenSuggestions_: Object,
    };
  }

  private suggestions_: string[];
  private selectableSuggestions_: string[];
  private hiddenSuggestions_: Set<string>;
  private debouncer_: Debouncer;
  private onResized_: () => void = () => {
    this.debouncer_ =
        Debouncer.debounce(this.debouncer_, timeOut.after(50), () => {
          this.getSelectableSuggestions_();
        });
  };

  override ready() {
    super.ready();
    IronA11yAnnouncer.requestAvailability();
    this.$.keys.target = this.$.suggestionSelector;
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.hiddenSuggestions_ = new Set();
    this.suggestions_ = [...SEA_PEN_SUGGESTIONS];
    this.shuffleSuggestions_();
    window.addEventListener('resize', this.onResized_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('resize', this.onResized_);
  }

  private getSelectableSuggestions_() {
    if (!isNonEmptyArray(this.suggestions_)) {
      return;
    }
    this.selectableSuggestions_ = this.suggestions_;
    afterNextRender(this, () => {
      const items = Array.from(
          this.shadowRoot!.querySelectorAll<CrButtonElement>('.suggestion'));
      const GAP = 10;  // 10px gap between suggestion chips.
      let remainingWidth = this.$.suggestionSelector.clientWidth - GAP;
      this.selectableSuggestions_ = this.suggestions_.filter((_, i) => {
        const itemWidth = items[i].clientWidth + GAP;
        remainingWidth -= itemWidth;
        return remainingWidth >= 0;
      });
    });
  }

  private onClickSuggestion_(event: Event&{model: {index: number}}) {
    const target = event.currentTarget as HTMLElement;
    const suggestion = target.textContent?.trim();
    assert(suggestion);
    this.dispatchEvent(new SeaPenSuggestionSelectedEvent(suggestion));
    logSuggestionClicked();

    // If there are fewer than 4 suggestions, shuffle them all instead of
    // removing one.
    if (SEA_PEN_SUGGESTIONS.length - this.hiddenSuggestions_.size < 4) {
      this.shuffleSuggestions_();
    } else {
      this.hiddenSuggestions_.add(suggestion);
      this.splice('suggestions_', event.model.index, 1);
      // Manually calls observer since splicing doesn't trigger it.
      this.onSuggestionsChanged_();
    }
  }

  private onShuffleClicked_() {
    logSuggestionShuffleClicked();
    this.shuffleSuggestions_();
    this.dispatchEvent(new IronAnnounceEvent(
        loadTimeData.getString('ariaAnnouncePromptSuggestionsShuffled')));
  }

  private shuffleSuggestions_() {
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

  private onSuggestionsChanged_() {
    this.getSelectableSuggestions_();
    requestAnimationFrame(() => {
      // The focused suggestion might be removed from the DOM once clicked. To
      // allow keyboard users to focus on the suggestions again, we add the
      // first suggestion back to tab order.
      const suggestions = this.$.suggestionSelector.items as HTMLElement[];
      const hasFocusableSuggestions =
          suggestions.some(el => el.getAttribute('tabindex') === '0');

      if (!hasFocusableSuggestions && suggestions.length > 0) {
        this.$.suggestionSelector.selectIndex(0);
        suggestions[0].setAttribute('tabindex', '0');
        suggestions[0].focus();
      }
    });
  }

  /** Handle keyboard navigation. */
  private onSuggestionKeyPressed_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.suggestionSelector;
    const prevSuggestion = selector.selectedItem as CrButtonElement;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous button.
    if (prevSuggestion) {
      prevSuggestion.removeAttribute('tabindex');
    }
    // Add focus state for new button.
    const currentSuggestion = selector.selectedItem as CrButtonElement;
    if (currentSuggestion) {
      currentSuggestion.setAttribute('tabindex', '0');
      currentSuggestion.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  private getSuggestionTabIndex_(index: number): string {
    return index === 0 ? '0' : '-1';
  }
}

customElements.define(SeaPenSuggestionsElement.is, SeaPenSuggestionsElement);
