// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/mwb_shared_icons.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrSearchFieldBehavior, CrSearchFieldBehaviorInterface} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrSearchFieldBehaviorInterface}
 */
const TabSearchSearchFieldBase =
    mixinBehaviors([ CrSearchFieldBehavior ], PolymerElement);

/** @polymer */
export class TabSearchSearchField extends TabSearchSearchFieldBase {

  static get is() {
    return 'tab-search-search-field';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Controls autofocus for the search field.
       */
      autofocus: {
        type: Boolean,
        value: false,
      },

      /**
       * Text that describes the resulting tabs currently present in the list.
       */
      searchResultText: {
        type: String,
        value: '',
      },

      /** @private {string} */
      shortcut_: {
        type: String,
        value: () => loadTimeData.getString('shortcutText'),
      },

      /** @private {string} */
      announceText_: {
        type: String,
        value: '',
      },
    };
  }

  /** @return {!HTMLInputElement} */
  getSearchInput() {
    return /** @type {!HTMLInputElement} */ (this.$.searchInput);
  }

  /**
   * Cause a text string to be announced by screen readers. Used for announcing
   * when the input field has focus for better compatibility with screen
   * readers.
   * @param {string} text The text that should be announced.
   */
  announce(text) {
    this.$.searchWrapper.append(
        this.$.searchWrapper.removeChild(this.$.inputAnnounce));
    if (this.announceText_ === text) {
      // A timeout is required when announcing duplicate text for certain
      // screen readers.
      this.announceText_ = '';
      setTimeout(() => {
        this.announceText_ = text;
      }, 100);
    } else {
      this.announceText_ = text;
    }
  }

  /**
   * Clear |announceText_| when focus leaves the input field to ensure the text
   * is not re-announced when focus returns to the input field.
   */
  onInputBlur_(text) {
    this.announceText_ = '';
  }

  /**
   * Do not schedule the timer from CrSearchFieldBehavior to make search more
   * responsive.
   * @override
   */
  onSearchTermInput() {
    this.hasSearchText = this.$.searchInput.value !== '';
    this.getSearchInput().dispatchEvent(
        new CustomEvent('search', {composed: true, detail: this.getValue()}));
  }
}

customElements.define(TabSearchSearchField.is, TabSearchSearchField);
