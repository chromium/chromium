// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './realbox_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {decodeString16} from '../utils.js';

// clang-format off
/**
 * Bitmap used to decode the value of search.mojom.ACMatchClassification style
 * field.
 * See components/omnibox/browser/autocomplete_match.h.
 * @enum {number}
 */
const ACMatchClassificationStyle = {
  NONE: 0,
  URL:   1 << 0,  // A URL.
  MATCH: 1 << 1,  // A match for the user's search term.
  DIM:   1 << 2,  // A "helper text".
};
// clang-format on

// Displays an autocomplete match similar to those in the Omnibox.
class RealboxMatchElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-match';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * Element's 'aria-label' attribute.
       * @type {string}
       */
      ariaLabel: {
        type: String,
        computed: `computeAriaLabel_(matchText_, removeButtonIsVisible_)`,
        reflectToAttribute: true,
      },

      /**
       * Whether the match features an image (as opposed to an icon or favicon).
       * @type {boolean}
       */
      hasImage: {
        type: Boolean,
        computed: `computeHasImage_(match)`,
        reflectToAttribute: true,
      },

      /**
       * @type {!search.mojom.AutocompleteMatch}
       */
      match: {
        type: Object,
      },

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as deletion, click, etc.
       * @type {number}
       */
      matchIndex: {
        type: Number,
        value: -1,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Rendered match contents based on autocomplete provided styling.
       * @type {string}
       * @private
       */
      contentsHtml_: {
        type: String,
        computed: `computeContentsHtml_(match)`,
      },

      /**
       * Rendered match description based on autocomplete provided styling.
       * @type {string}
       * @private
       */
      descriptionHtml_: {
        type: String,
        computed: `computeDescriptionHtml_(match)`,
      },

      /**
       * The text content of match used to calculate the 'aria-label' attributes
       * of the element as well as the remove button.
       * @type {string}
       * @private
       */
      matchText_: {
        type: String,
        computed: `computeMatchText_(match)`,
      },

      /**
       * Remove button's 'aria-label' attribute.
       * @type {string}
       * @private
       */
      removeButtonAriaLabel_: {
        type: String,
        computed: `computeRemoveButtonAriaLabel_(matchText_)`,
      },

      /**
       * @type {boolean}
       * @private
       */
      removeButtonIsVisible_: {
        type: Boolean,
        computed: `computeRemoveButtonIsVisible_(match)`,
      },

      /**
       * @type {string}
       * @private
       */
      removeButtonTitle_: {
        type: String,
        value: () => loadTimeData.getString('removeSuggestion'),
      },

      /**
       * Used to separate the contents from the description.
       * @type {string}
       * @private
       */
      separatorText_: {
        type: String,
        computed: `computeSeparatorText_(match)`,
      },
    };
  }

  ready() {
    super.ready();

    this.addEventListener('click', this.onMatchClick_.bind(this));
    this.addEventListener('focusin', this.onMatchFocusin_.bind(this));
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @param {!Event} e
   * @private
   */
  onMatchClick_(e) {
    if (e.button > 1) {
      // Only handle main (generally left) and middle button presses.
      return;
    }

    this.dispatchEvent(new CustomEvent('match-click', {
      bubbles: true,
      composed: true,
      detail: {index: this.matchIndex, event: e},
    }));

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  /**
   * @param {!Event} e
   * @private
   */
  onMatchFocusin_(e) {
    this.dispatchEvent(new CustomEvent('match-focusin', {
      bubbles: true,
      composed: true,
      detail: this.matchIndex,
    }));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveButtonClick_(e) {
    if (e.button !== 0) {
      // Only handle main (generally left) button presses.
      return;
    }
    this.dispatchEvent(new CustomEvent('match-remove', {
      bubbles: true,
      composed: true,
      detail: this.matchIndex,
    }));

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveButtonMouseDown_(e) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @return {string}
   * @private
   */
  computeMatchText_() {
    if (!this.match) {
      return '';
    }
    const contents = decodeString16(this.match.contents);
    const description = decodeString16(this.match.description);
    return this.match.swapContentsAndDescription ?
        description + this.separatorText_ + contents :
        contents + this.separatorText_ + description;
  }

  /**
   * @return {string}
   * @private
   */
  computeAriaLabel_() {
    return this.removeButtonIsVisible_ ?
        loadTimeData.getStringF('removeSuggestionA11ySuffix', this.matchText_) :
        this.matchText_;
  }

  /**
   * @return {string}
   * @private
   */
  computeContentsHtml_() {
    if (!this.match) {
      return '';
    }
    const match = this.match;
    return match.swapContentsAndDescription ?
        this.renderTextWithClassifications_(
                decodeString16(match.description), match.descriptionClass)
            .innerHTML :
        this.renderTextWithClassifications_(
                decodeString16(match.contents), match.contentsClass)
            .innerHTML;
  }

  /**
   * @return {string}
   * @private
   */
  computeDescriptionHtml_() {
    if (!this.match) {
      return '';
    }
    const match = this.match;
    return match.swapContentsAndDescription ?
        this.renderTextWithClassifications_(
                decodeString16(match.contents), match.contentsClass)
            .innerHTML :
        this.renderTextWithClassifications_(
                decodeString16(match.description), match.descriptionClass)
            .innerHTML;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeHasImage_() {
    return this.match && !!this.match.imageUrl;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeRemoveButtonIsVisible_() {
    return this.match && this.match.supportsDeletion;
  }

  /**
   * @return {string}
   * @private
   */
  computeRemoveButtonAriaLabel_() {
    return loadTimeData.getStringF(
        'removeSuggestionA11yPrefix', this.matchText_);
  }

  /**
   * @return {string}
   * @private
   */
  computeSeparatorText_() {
    return this.match && decodeString16(this.match.description) ?
        loadTimeData.getString('realboxSeparator') :
        '';
  }

  /**
   * Decodes the ACMatchClassificationStyle enteries encoded in the given
   * search.mojom.ACMatchClassification style field, maps each entry to a CSS
   * class and returns them.
   * @param {number} style
   * @return {!Array<string>}
   */
  convertClassificationStyleToCSSClasses_(style) {
    const classes = [];
    if (style & ACMatchClassificationStyle.DIM) {
      classes.push('dim');
    }
    if (style & ACMatchClassificationStyle.MATCH) {
      classes.push('match');
    }
    if (style & ACMatchClassificationStyle.URL) {
      classes.push('url');
    }
    return classes;
  }

  /**
   * @param {string} text
   * @param {!Array<string>} classes
   * @return {!Element}
   */
  createSpanWithClasses_(text, classes) {
    const span = document.createElement('span');
    if (classes.length) {
      span.classList.add(...classes);
    }
    span.textContent = text;
    return span;
  }

  /**
   * Renders |text| based on the given search.mojom.ACMatchClassification(s)
   * Each classification contains an 'offset' and an encoded list of styles for
   * styling a substring starting with the 'offset' and ending with the next.
   * @param {string} text
   * @param {!Array<!search.mojom.ACMatchClassification>} classifications
   * @return {!Element} A <span> with <span> children for each styled substring.
   */
  renderTextWithClassifications_(text, classifications) {
    return classifications
        .map(({offset, style}, index) => {
          const next = classifications[index + 1] || {offset: text.length};
          const subText = text.substring(offset, next.offset);
          const classes = this.convertClassificationStyleToCSSClasses_(style);
          return this.createSpanWithClasses_(subText, classes);
        })
        .reduce((container, currentElement) => {
          container.appendChild(currentElement);
          return container;
        }, document.createElement('span'));
  }
}

customElements.define(RealboxMatchElement.is, RealboxMatchElement);
