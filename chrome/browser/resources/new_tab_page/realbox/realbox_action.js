// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {decodeString16} from '../utils.js';

// Displays an action associated with AutocompleteMatch (i.e. Clear
// Browsing History, etc.)
class RealboxActionElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-action';
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
       * @type {!realbox.mojom.Action}
       */
      action: {
        type: Object,
      },

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as click, keyboard events etc.
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
       * Element's 'aria-label' attribute.
       * @type {string}
       */
      ariaLabel: {
        type: String,
        computed: `computeAriaLabel_(action)`,
        reflectToAttribute: true,
      },

      /**
       * Rendered hint from action.
       * @private {string}
       */
      hintHtml_: {
        type: String,
        computed: `computeHintHtml_(action)`,
      },
    };
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @return {string}
   * @private
   */
  computeAriaLabel_() {
    if (this.action.accessibilityHint) {
      return decodeString16(this.action.accessibilityHint);
    }
    return '';
  }

  /**
   * @return {string}
   * @private
   */
  computeHintHtml_() {
    if (this.action.hint) {
      return decodeString16(this.action.hint);
    }
    return '';
  }
}

customElements.define(RealboxActionElement.is, RealboxActionElement);
