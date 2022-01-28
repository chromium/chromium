// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_content.js';
import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HelpContentList, HelpContentProviderInterface} from './feedback_types.js';
import {getHelpContentProvider} from './mojo_interface_provider.js';

/**
 * The minimum number of characters added or deleted to start a new search for
 * help content.
 * @type {number}
 */
const MIN_CHARS_COUNT = 3;

/**
 * The maximum number of help contents wanted per search.
 *  @type {number}
 */
const MAX_RESULTS = 5;

/**
 * @fileoverview
 * 'search-page' is the first step of the feedback tool. It displays live help
 *  contents relevant to the text entered by the user.
 */
export class SearchPageElement extends PolymerElement {
  static get is() {
    return 'search-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();

    /**
     * Record the most recent number of characters in the input for which a
     * search has been attempted.
     * @private {number}
     */
    this.lastCharCount_ = 0;

    /** @private {!HelpContentProviderInterface} */
    this.helpContentProvider_ = getHelpContentProvider();

    /** @private {!HelpContentList} */
    this.helpContentList_ = [];
  }

  /**
   * @private
   */
  handleInputChanged_(e) {
    const newInput = e.target.value;
    // Get the number of characters in the input.
    const newCharCount = [...newInput].length;

    if (Math.abs(newCharCount - this.lastCharCount_) >= MIN_CHARS_COUNT) {
      this.lastCharCount_ = newCharCount;
      this.fetchHelpContent_(newInput);
    }
  }

  /**
   * @param {string} query
   * @private
   */
  fetchHelpContent_(query) {
    this.helpContentProvider_.getHelpContents(query, MAX_RESULTS)
        .then((/** !HelpContentList */ newContentList) => {
          this.helpContentList_ = newContentList;
        });
  }
}

customElements.define(SearchPageElement.is, SearchPageElement);
