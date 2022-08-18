// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_content.js';
import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeedbackFlowState} from './feedback_flow.js';
import {HelpContentList, HelpContentProviderInterface, SearchRequest, SearchResponse, SearchResult} from './feedback_types.js';
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

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SearchPageElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SearchPageElement extends SearchPageElementBase {
  static get is() {
    return 'search-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      descriptionTemplate: {
        type: String,
        readonly: true,
        observer: SearchPageElement.prototype.descriptionTemplateChanged_,
      },
    };
  }

  constructor() {
    super();

    /** @type {string} */
    this.descriptionTemplate = '';

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

    // Set focus on the input field after iframe is loaded.
    this.iframeLoaded_.then(() => this.focusInputElement());

    /** @private {?HTMLIFrameElement} */
    this.iframe_ = null;

    /**
     * The content list received when query is empty.
     * @private {!HelpContentList|undefined}
     */
    this.popularHelpContentList_;
  }

  ready() {
    super.ready();

    this.iframe_ = /** @type {HTMLIFrameElement} */ (
        this.shadowRoot.querySelector('iframe'));
    // Fetch popular help contents with empty query.
    this.fetchHelpContent_(/* query= */ '');
  }

  /**
   * @param {!Event} e
   * @private
   */
  handleInputChanged_(e) {
    const newInput = e.target.value;
    // Get the number of characters in the input.
    const newCharCount = [...newInput].length;

    if (newCharCount > 0) {
      this.hideError_();
    }

    if (Math.abs(newCharCount - this.lastCharCount_) >= MIN_CHARS_COUNT) {
      this.lastCharCount_ = newCharCount;
      this.fetchHelpContent_(newInput);
    }
  }

  /**
   * @param {string} query
   * @private
   */
  async fetchHelpContent_(query) {
    if (!this.iframe_) {
      console.warn('untrusted iframe is not found');
      return;
    }

    /** @type {!SearchRequest} */
    const request = {
      query: stringToMojoString16(query),
      maxResults: MAX_RESULTS,
    };

    /** @type boolean */
    const isQueryEmpty = (query === '');

    /** @type boolean */
    let isPopularContent;

    /** @type {{response: !SearchResponse}} */
    let response;

    if (isQueryEmpty) {
      // Load popular help content if they are not loaded before.
      if (this.popularHelpContentList_ === undefined) {
        response = await this.helpContentProvider_.getHelpContents(request);
        this.popularHelpContentList_ = response.response.results;
      }
      isPopularContent = true;
    } else {
      response = await this.helpContentProvider_.getHelpContents(request);
      isPopularContent = (response.response.totalResults === 0);
    }

    /** @type {!SearchResult} */
    const data = {
      contentList: /** @type {!HelpContentList} */ (
          isPopularContent ? this.popularHelpContentList_ :
                             response.response.results),
      isQueryEmpty: isQueryEmpty,
      isPopularContent: isPopularContent,
    };

    // Wait for the iframe to complete loading before postMessage.
    await this.iframeLoaded_;
    // TODO(xiangdongkong): Use Mojo to communicate with untrusted page.
    this.iframe_.contentWindow.postMessage(data, OS_FEEDBACK_UNTRUSTED_ORIGIN);
  }

  /**
   * @return {!HTMLTextAreaElement}
   * @private
   */
  getInputElement_() {
    return /** @type {!HTMLTextAreaElement} */ (
        this.shadowRoot.querySelector('#descriptionText'));
  }

  /**
   * Focus on the textarea element.
   */
  focusInputElement() {
    this.getInputElement_().focus();
  }

  /**
   * @private
   */
  onInputInvalid_() {
    this.showError_();
    this.focusInputElement();
  }

  /**
   * @return {!HTMLElement}
   * @private
   */
  getDescriptionTextElement_() {
    return /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#descriptionText'));
  }

  /**
   * @return {!HTMLElement}
   * @private
   */
  getErrorElement_() {
    return /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#descriptionEmptyError'));
  }

  /**
   * @private
   */
  showError_() {
    // TODO(xiangdongkong): Change the textarea's aria-labelledby to ensure the
    // screen reader does (or doesn't) read the error, as appropriate.
    // If it does read the error, it should do so _before_ it reads the normal
    // description.
    const errorElement = this.getErrorElement_();
    errorElement.hidden = false;
    errorElement.setAttribute('aria-hidden', false);

    const descriptionTextElement = this.getDescriptionTextElement_();
    descriptionTextElement.classList.add('has-error');
  }

  /**
   * @private
   */
  hideError_() {
    const errorElement = this.getErrorElement_();
    errorElement.hidden = true;
    errorElement.setAttribute('aria-hidden', true);

    const descriptionTextElement = this.getDescriptionTextElement_();
    descriptionTextElement.classList.remove('has-error');
  }

  /**
   * @returns {string}
   * @protected
   */
  feedbackWritingGuidanceUrl_() {
    // TODO(xiangdongkong): append ?hl={the application locale} to the url.
    const url = 'https://support.google.com/chromebook/answer/2982029';
    return url;
  }

  /**
   * @param {!Event} e
   * @private
   */
  handleContinueButtonClicked_(e) {
    e.stopPropagation();

    const textInput = this.getInputElement_().value;
    if (textInput.length === 0) {
      this.onInputInvalid_();
    } else {
      this.dispatchEvent(new CustomEvent('continue-click', {
        composed: true,
        bubbles: true,
        detail:
            {currentState: FeedbackFlowState.SEARCH, description: textInput},
      }));
    }
  }

  /**
   * @param {string} text
   */
  setDescription(text) {
    this.getInputElement_().value = text;
  }

  /**
   * @param {string} currentTemplate
   * @protected
   */
  descriptionTemplateChanged_(currentTemplate) {
    this.getInputElement_().value = currentTemplate;
  }
}

customElements.define(SearchPageElement.is, SearchPageElement);
