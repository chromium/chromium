// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_content.js';
import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HelpContentProviderInterface, SearchRequest, SearchResponse} from './feedback_types.js';
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
 * The host of untrusted child page.
 * @type {string}
 */
export const OS_FEEDBACK_UNTRUSTED_ORIGIN = 'chrome-untrusted://os-feedback';

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

    /**
     * The event handler called when the iframe is loaded. It is set in the
     * html.
     * @private {function()}
     */
    this.resolveIframeLoaded_;

    /**
     * A promise that resolves when the iframe loading is completed.
     * @private {Promise}
     */
    this.iframeLoaded_ = new Promise(resolve => {
      this.resolveIframeLoaded_ = resolve;
    });

    /** @private {?HTMLIFrameElement} */
    this.iframe_ = null;
  }

  ready() {
    super.ready();

    this.iframe_ = /** @type {HTMLIFrameElement} */ (
        this.shadowRoot.querySelector('iframe'));
  }
  /**
   *
   * @private
   */
  handleInputChanged_(e) {
    const newInput = e.target.value;
    // Get the number of characters in the input.
    const newCharCount = [...newInput].length;

    if (Math.abs(newCharCount - this.lastCharCount_) >= MIN_CHARS_COUNT) {
      this.lastCharCount_ = newCharCount;

      /** @type {!SearchRequest} */
      const request = {
        query: stringToMojoString16(newInput),
        maxResults: MAX_RESULTS,
      };

      this.fetchHelpContent_(request);
    }
  }

  /**
   * @param {!SearchRequest} request
   * @private
   */
  fetchHelpContent_(request) {
    this.helpContentProvider_.getHelpContents(request).then(
        /**  {{response: !SearchResponse}} */ (response) => {
          if (!this.iframe_) {
            console.warn('untrusted iframe is not found');
            return;
          }

          const data = {
            response: response.response,
          };

          // Wait for the iframe to complete loading before postMessage.
          this.iframeLoaded_.then(() => {
            // TODO(xiangdongkong): Use Mojo to communicate with untrusted page.
            this.iframe_.contentWindow.postMessage(
                data, OS_FEEDBACK_UNTRUSTED_ORIGIN);
          });
        });
  }
}

customElements.define(SearchPageElement.is, SearchPageElement);
