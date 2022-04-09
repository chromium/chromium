// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './confirmation_page.js';
import './search_page.js';
import './share_data_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FeedbackContext, FeedbackServiceProviderInterface} from './feedback_types.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';

/**
 * Enum for feedback flow states.
 * @enum {string}
 */
export const FeedbackFlowState = {
  SEARCH: 'searchPage',
  SHARE_DATA: 'shareDataPage',
  CONFIRMATION: 'confirmationPage',
};

/**
 * @fileoverview
 * 'feedback-flow' manages the navigation among the steps to be taken.
 */
export class FeedbackFlowElement extends PolymerElement {
  static get is() {
    return 'feedback-flow';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      currentState_: {type: FeedbackFlowState},
      feedbackContext_: {type: FeedbackContext, readonly: false, notify: true},
    };
  }

  constructor() {
    super();

    /**
     * The id of an element on the page that is currently shown.
     * @protected {FeedbackFlowState}
     */
    this.currentState_ = FeedbackFlowState.SEARCH;

    /**
     * The feedback context.
     * @type {?FeedbackContext}
     * @protected
     */
    this.feedbackContext_ = null;

    /** @private {!FeedbackServiceProviderInterface} */
    this.feedbackServiceProvider_ = getFeedbackServiceProvider();
  }

  ready() {
    super.ready();

    this.feedbackServiceProvider_.getFeedbackContext().then((response) => {
      this.feedbackContext_ = response.feedbackContext;
    });
  }

  /**
   * @param {!Event} event
   * @protected
   */
  handleContinueClick_(event) {
    switch (event.detail.currentState) {
      case FeedbackFlowState.SEARCH:
        this.currentState_ = FeedbackFlowState.SHARE_DATA;
        break;
      default:
        console.warn('unexpected state: ', event.detail.currentState);
    }
  }

  /**
   * @param {!Event} event
   * @protected
   */
  handleGoBackClick_(event) {
    switch (event.detail.currentState) {
      case FeedbackFlowState.SHARE_DATA:
        this.currentState_ = FeedbackFlowState.SEARCH;
        break;
      default:
        console.warn('unexpected state: ', event.detail.currentState);
    }
  }

  /**
   * @param {!FeedbackFlowState} newState
   */
  setCurrentStateForTesting(newState) {
    this.currentState_ = newState;
  }
}

customElements.define(FeedbackFlowElement.is, FeedbackFlowElement);
