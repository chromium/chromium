// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './confirmation_page.js';
import './search_page.js';
import './share_data_page.js';
import './strings.m.js';

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeedbackAppExitPath, FeedbackContext, FeedbackServiceProviderInterface, Report, SendReportStatus} from './feedback_types.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';

/**
 * The host of untrusted child page.
 * @type {string}
 */
const OS_FEEDBACK_UNTRUSTED_ORIGIN = 'chrome-untrusted://os-feedback';

/**
 * The id of help-content-clicked message.
 * @type {string}
 */
const HELP_CONTENT_CLICKED = 'help-content-clicked';

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
 * Enum for reserved query parameters used by feedback source to provide
 * addition context to final report.
 * @enum {string}
 */
export const AdditionalContextQueryParam = {
  DESCRIPTION_TEMPLATE: 'description_template',
  EXTRA_DIAGNOSTICS: 'extra_diagnostics',
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

    /**
     * The description entered by the user. It is set when the user clicks the
     * next button on the search page.
     * @type {string}
     * @private
     */
    this.description_;

    /**
     * The description template provided source application to help user write
     * feedback.
     * @type {string}
     * @protected
     */
    this.descriptionTemplate_;

    /**
     * The status of sending report.
     * @type {?SendReportStatus}
     * @private
     */
    this.sendReportStatus_;

    /**
     * Whether user clicks the help content or not.
     * @type {boolean}
     * @private
     */
    this.helpContentClicked_ = false;
  }

  ready() {
    super.ready();

    this.feedbackServiceProvider_.getFeedbackContext().then((response) => {
      this.feedbackContext_ = response.feedbackContext;
      this.setAdditionalContextFromQueryParams_();
    });

    window.addEventListener('message', event => {
      if (event.data.id !== HELP_CONTENT_CLICKED) {
        return;
      }
      if (event.origin !== OS_FEEDBACK_UNTRUSTED_ORIGIN) {
        console.error('Unknown origin: ' + event.origin);
        return;
      }
      this.helpContentClicked_ = true;
    });

    window.addEventListener('beforeunload', event => {
      event.preventDefault();

      // TODO(longbowei): Handle kQuitNoResultFound case.
      switch (this.currentState_) {
        case FeedbackFlowState.SEARCH:
          this.recordExitPath_(
              FeedbackAppExitPath.kQuitSearchPageHelpContentClicked,
              FeedbackAppExitPath.kQuitSearchPageNoHelpContentClicked);
          break;
        case FeedbackFlowState.SHARE_DATA:
          this.recordExitPath_(
              FeedbackAppExitPath.kQuitShareDataPageHelpContentClicked,
              FeedbackAppExitPath.kQuitShareDataPageNoHelpContentClicked);
          break;
        case FeedbackFlowState.CONFIRMATION:
          this.recordExitPath_(
              FeedbackAppExitPath.kSuccessHelpContentClicked,
              FeedbackAppExitPath.kSuccessNoHelpContentClicked);
          break;
      }
    });
  }

  /**
   * @private
   */
  fetchScreenshot_() {
    const shareDataPage = this.shadowRoot.querySelector('share-data-page');
    // Fetch screenshot if not fetched before.
    if (!shareDataPage.screenshotUrl) {
      this.feedbackServiceProvider_.getScreenshotPng().then((response) => {
        if (response.pngData.length > 0) {
          const blob = new Blob(
              [Uint8Array.from(response.pngData)], {type: 'image/png'});
          const imageUrl = URL.createObjectURL(blob);
          shareDataPage.screenshotUrl = imageUrl;
        }
      });
    }
  }

  /**
   * Sets additional context passed from RequestFeedbackFlow as part of the URL.
   * See `AdditionalContextQueryParam` for valid query parameters.
   * @private
   */
  setAdditionalContextFromQueryParams_() {
    const params = new URLSearchParams(window.location.search);
    const extraDiagnostics =
        params.get(AdditionalContextQueryParam.EXTRA_DIAGNOSTICS);
    this.feedbackContext_.extraDiagnostics =
        extraDiagnostics ? decodeURIComponent(extraDiagnostics) : '';
    const descriptionTemplate =
        params.get(AdditionalContextQueryParam.DESCRIPTION_TEMPLATE);
    this.descriptionTemplate_ =
        descriptionTemplate && descriptionTemplate.length > 0 ?
        decodeURIComponent(descriptionTemplate) :
        '';
  }

  /**
   * @param {!Event} event
   * @protected
   */
  handleContinueClick_(event) {
    switch (event.detail.currentState) {
      case FeedbackFlowState.SEARCH:
        this.currentState_ = FeedbackFlowState.SHARE_DATA;
        this.description_ = event.detail.description;
        this.fetchScreenshot_();
        break;
      case FeedbackFlowState.SHARE_DATA:
        /** @type {!Report} */
        const report = event.detail.report;
        report.description = stringToMojoString16(this.description_);

        // TODO(xiangdongkong): Show a spinner or the like for sendReport could
        // take a while.
        this.feedbackServiceProvider_.sendReport(report).then((response) => {
          this.currentState_ = FeedbackFlowState.CONFIRMATION;
          this.sendReportStatus_ = response.status;
        });
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
        this.navigateToSearchPage_();
        break;
      case FeedbackFlowState.CONFIRMATION:
        // Remove the text from previous search.
        const searchPage = this.shadowRoot.querySelector('search-page');
        searchPage.setDescription(/*text=*/ '');

        // Re-enable the send button in share data page.
        const shareDataPage = this.shadowRoot.querySelector('share-data-page');
        shareDataPage.reEnableSendReportButton();

        this.navigateToSearchPage_();
        break;
      default:
        console.warn('unexpected state: ', event.detail.currentState);
    }
  }

  /** @private */
  navigateToSearchPage_() {
    this.currentState_ = FeedbackFlowState.SEARCH;
    this.shadowRoot.querySelector('search-page').focusInputElement();
  }

  /**
   * @param {!FeedbackAppExitPath} pathHelpContentClicked
   * @param {!FeedbackAppExitPath} pathNoHelpContentClicked
   * @private
   */
  recordExitPath_(pathHelpContentClicked, pathNoHelpContentClicked) {
    this.helpContentClicked_ ?
        this.feedbackServiceProvider_.recordExitPath(pathHelpContentClicked) :
        this.feedbackServiceProvider_.recordExitPath(pathNoHelpContentClicked);
  }

  /**
   * @param {!FeedbackFlowState} newState
   */
  setCurrentStateForTesting(newState) {
    this.currentState_ = newState;
  }

  /**
   * @param {!SendReportStatus} status
   */
  setSendReportStatusForTesting(status) {
    this.sendReportStatus_ = status;
  }

  /**
   * @param {string} text
   */
  setDescriptionForTesting(text) {
    this.description_ = text;
  }

  /**
   * @param {boolean} helpContentClicked
   */
  setHelpContentClickedForTesting(helpContentClicked) {
    this.helpContentClicked_ = helpContentClicked;
  }
}

customElements.define(FeedbackFlowElement.is, FeedbackFlowElement);
