// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './confirmation_page.js';
import './search_page.js';
import './share_data_page.js';
import './strings.m.js';

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeedbackAppExitPath, FeedbackAppHelpContentOutcome, FeedbackAppPreSubmitAction, FeedbackContext, FeedbackServiceProviderInterface, Report, SendReportStatus} from './feedback_types.js';
import {showScrollingEffectOnStart, showScrollingEffects} from './feedback_utils.js';
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
 * Enum for actions on search page.
 * @enum {string}
 */
export const SearchPageAction = {
  CONTINUE: 'continue',
  QUIT: 'quit',
};

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
  DESCRIPTION_PLACEHOLDER_TEXT: 'description_placeholder_text',
  EXTRA_DIAGNOSTICS: 'extra_diagnostics',
  CATEGORY_TAG: 'category_tag',
  PAGE_URL: 'page_url',
  FROM_ASSISTANT: 'from_assistant',
  FROM_SETTINGS_SEARCH: 'from_settings_search',
};

/**
 * Builds a RegExp that matches one of the given words. Each word has to match
 * at word boundary and is not at the end of the tested string. For example,
 * the word "SIM" would match the string "I have a sim card issue" but not
 * "I have a simple issue" nor "I have a sim" (because the user might not have
 * finished typing yet).
 * @param {!Array<!string>} words
 * @return {!RegExp}
 * @protected
 */
export function buildWordMatcher(words) {
  return new RegExp(
      words.map((word) => '\\b' + word + '\\b[^$]').join('|'), 'i');
}

/**
 * Regular expression to check for all variants of blu[e]toot[h] with or
 * without space between the words; for BT when used as an individual word,
 * or as two individual characters, and for BLE, BlueZ, and Floss when used
 * as an individual word. Case insensitive matching.
 * @type {!RegExp}
 * @protected
 */
export const btRegEx = new RegExp(
    'blu[e]?[ ]?toot[h]?|\\bb[ ]?t\\b|\\bble\\b|\\bfloss\\b|\\bbluez\\b', 'i');

/**
 * Regular expression to check for all strings indicating that a user can't
 * connect to a HID or Audio device.
 * Sample strings this will match:
 * "I can't connect the speaker!",
 * "The keyboard has connection problem."
 * @type {!RegExp}
 * @protected
 */
const cantConnectRegEx = new RegExp(
    '((headphone|keyboard|mouse|speaker)((?!(connect|pair)).*)(connect|pair))' +
        '|((connect|pair).*(headphone|keyboard|mouse|speaker))',
    'i');

/**
 * Regular expression to check for "tether" or "tethering". Case insensitive
 * matching.
 * @type {!RegExp}
 * @protected
 */
const tetherRegEx = new RegExp('tether(ing)?', 'i');

/**
 * Regular expression to check for "Smart (Un)lock" or "Easy (Un)lock" with
 * or without space between the words. Case insensitive matching.
 * @type {!RegExp}
 * @protected
 */
const smartLockRegEx = new RegExp('(smart|easy)[ ]?(un)?lock', 'i');

/**
 * Regular expression to check for keywords related to Nearby Share like
 * "nearby (share)" or "phone (hub)".
 * Case insensitive matching.
 * @type {!RegExp}
 * @protected
 */
const nearbyShareRegEx = new RegExp('nearby|phone', 'i');

/**
 * Regular expression to check for keywords related to Fast Pair like
 * "fast pair".
 * Case insensitive matching.
 * @type {!RegExp}
 * @protected
 */
const fastPairRegEx = new RegExp('fast[ ]?pair', 'i');

/**
 * Regular expression to check for Bluetooth device specific keywords.
 * @type {!RegExp}
 * @protected
 */
const btDeviceRegEx =
    buildWordMatcher(['apple', 'allegro', 'pixelbud', 'microsoft', 'sony']);

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

    /**
     * Whether to show the bluetooth Logs checkbox in share data page.
     * @type {boolean}
     */
    this.shouldShowBluetoothCheckbox_;

    /**
     * Whether to show the assistant checkbox in share data page.
     * @type {boolean}
     */
    this.shouldShowAssistantCheckbox_;

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
     * The descripiton placeholder text is used to give the user a hint on how
     * to write the description. Some apps, such as the Camera app can use a
     * custom placeholder.
     * @type {string}
     * @protected
     */
    this.descriptionPlaceholderText_;

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

    /**
     * To avoid helpContentOutcome metric emit more than one time.
     * @type {boolean}
     * @private
     */
    this.helpContentOutcomeMetricEmitted_ = false;

    /**
     * Number of results returned in each search.
     * @type {number}
     * @private
     */
    this.helpContentSearchResultCount_;

    /**
     * Whether there is no help content shown(offline or search is down).
     * @type {boolean}
     * @private
     */
    this.noHelpContentDisplayed_;
  }

  ready() {
    super.ready();

    this.feedbackServiceProvider_.getFeedbackContext().then((response) => {
      this.feedbackContext_ = response.feedbackContext;
      this.setAdditionalContextFromQueryParams_();
      this.shouldShowAssistantCheckbox_ = !!this.feedbackContext_ &&
          this.feedbackContext_.isInternalAccount &&
          this.feedbackContext_.fromAssistant;
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
      this.feedbackServiceProvider_.recordPreSubmitAction(
          FeedbackAppPreSubmitAction.kViewedHelpContent);
      this.feedbackServiceProvider_.recordHelpContentSearchResultCount(
          this.helpContentSearchResultCount_);
    });

    window.addEventListener('beforeunload', event => {
      event.preventDefault();

      switch (this.currentState_) {
        case FeedbackFlowState.SEARCH:
          this.recordExitPath_(
              FeedbackAppExitPath.kQuitSearchPageHelpContentClicked,
              FeedbackAppExitPath.kQuitSearchPageNoHelpContentClicked);
          if (!this.helpContentOutcomeMetricEmitted_) {
            this.recordHelpContentOutcome_(
                SearchPageAction.QUIT,
                FeedbackAppHelpContentOutcome.kQuitHelpContentClicked,
                FeedbackAppHelpContentOutcome.kQuitNoHelpContentClicked);
            this.helpContentOutcomeMetricEmitted_ = true;
          }
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

    this.style.setProperty(
        '--window-height', window.innerHeight.toString() + 'px');
    window.addEventListener('resize', (event) => {
      this.style.setProperty(
          '--window-height', window.innerHeight.toString() + 'px');
      let page = null;
      switch (this.currentState_) {
        case FeedbackFlowState.SEARCH:
          page = this.shadowRoot.querySelector('search-page');
          break;
        case FeedbackFlowState.SHARE_DATA:
          page = this.shadowRoot.querySelector('share-data-page');
          break;
        case FeedbackFlowState.CONFIRMATION:
          page = this.shadowRoot.querySelector('confirmation-page');
          break;
        default:
          console.warn('unexpected state: ', this.currentState_);
      }
      showScrollingEffects(event, page);
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
    const descriptionPlaceholderText =
        params.get(AdditionalContextQueryParam.DESCRIPTION_PLACEHOLDER_TEXT);
    this.descriptionPlaceholderText_ =
        descriptionPlaceholderText && descriptionPlaceholderText.length > 0 ?
        decodeURIComponent(descriptionPlaceholderText) :
        '';
    const categoryTag = params.get(AdditionalContextQueryParam.CATEGORY_TAG);
    this.feedbackContext_.categoryTag =
        categoryTag ? decodeURIComponent(categoryTag) : '';
    const pageUrl = params.get(AdditionalContextQueryParam.PAGE_URL);
    if (pageUrl) {
      this.feedbackContext_.pageUrl = {url: pageUrl};
    }
    const fromAssistant =
        params.get(AdditionalContextQueryParam.FROM_ASSISTANT);
    this.feedbackContext_.fromAssistant = !!fromAssistant;
    const fromSettingsSearch =
        params.get(AdditionalContextQueryParam.FROM_SETTINGS_SEARCH);
    this.set('feedbackContext_.fromSettingsSearch', !!fromSettingsSearch);
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
        this.shouldShowBluetoothCheckbox_ = this.feedbackContext_ !== null &&
            this.feedbackContext_.isInternalAccount &&
            this.isDescriptionRelatedToBluetooth(this.description_);
        this.fetchScreenshot_();
        const shareDataPage = this.shadowRoot.querySelector('share-data-page');
        shareDataPage.focusScreenshotCheckbox();
        showScrollingEffectOnStart(shareDataPage);

        if (!this.helpContentOutcomeMetricEmitted_) {
          this.recordHelpContentOutcome_(
              SearchPageAction.CONTINUE,
              FeedbackAppHelpContentOutcome.kContinueHelpContentClicked,
              FeedbackAppHelpContentOutcome.kContinueNoHelpContentClicked);
          this.helpContentOutcomeMetricEmitted_ = true;
        }
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
          const confirmationPage =
              this.shadowRoot.querySelector('confirmation-page');
          confirmationPage.focusPageTitle();
          showScrollingEffectOnStart(confirmationPage);
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
        showScrollingEffectOnStart(searchPage);

        // Re-enable the send button in share data page.
        const shareDataPage = this.shadowRoot.querySelector('share-data-page');
        shareDataPage.reEnableSendReportButton();

        // Re-enable helpContentOutcomeMetric to be emitted in search page.
        this.helpContentOutcomeMetricEmitted_ = false;

        this.navigateToSearchPage_();
        break;
      default:
        console.warn('unexpected state: ', event.detail.currentState);
    }
  }

  /** @private */
  navigateToSearchPage_() {
    this.currentState_ = FeedbackFlowState.SEARCH;
    const searchPage = this.shadowRoot.querySelector('search-page');
    searchPage.focusInputElement();
    showScrollingEffectOnStart(searchPage);
  }

  /**
   * @param {!SearchPageAction} action
   * @param {!FeedbackAppHelpContentOutcome} outcomeHelpContentClicked
   * @param {!FeedbackAppHelpContentOutcome} outcomeNoHelpContentClicked
   * @private
   */
  recordHelpContentOutcome_(
      action, outcomeHelpContentClicked, outcomeNoHelpContentClicked) {
    if (this.noHelpContentDisplayed_) {
      action == SearchPageAction.CONTINUE ?
          this.feedbackServiceProvider_.recordHelpContentOutcome(
              FeedbackAppHelpContentOutcome.kContinueNoHelpContentDisplayed) :
          this.feedbackServiceProvider_.recordHelpContentOutcome(
              FeedbackAppHelpContentOutcome.kQuitNoHelpContentDisplayed);
      return;
    }

    this.helpContentClicked_ ?
        this.feedbackServiceProvider_.recordHelpContentOutcome(
            outcomeHelpContentClicked) :
        this.feedbackServiceProvider_.recordHelpContentOutcome(
            outcomeNoHelpContentClicked);
  }

  /**
   * @param {!FeedbackAppExitPath} pathHelpContentClicked
   * @param {!FeedbackAppExitPath} pathNoHelpContentClicked
   * @private
   */
  recordExitPath_(pathHelpContentClicked, pathNoHelpContentClicked) {
    if (this.noHelpContentDisplayed_) {
      this.feedbackServiceProvider_.recordExitPath(
          FeedbackAppExitPath.kQuitNoHelpContentDisplayed);
      return;
    }
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

  /**
   * @param {boolean} noHelpContentDisplayed
   */
  setNoHelpContentDisplayedForTesting(noHelpContentDisplayed) {
    this.noHelpContentDisplayed_ = noHelpContentDisplayed;
  }

  /**
   * Checks if any keywords related to bluetooth have been typed. If they are,
   * we show the bluetooth logs option, otherwise hide it.
   * @return {boolean}
   * @param {!string} textInput The input text for the description textarea.
   * @protected
   */
  isDescriptionRelatedToBluetooth(textInput) {
    /**
     * If the user is not signed in with a internal google account, the
     * bluetooth checkbox should be hidden and skip the relative check.
     */
    const isRelatedToBluetooth = btRegEx.test(textInput) ||
        cantConnectRegEx.test(textInput) || tetherRegEx.test(textInput) ||
        smartLockRegEx.test(textInput) || nearbyShareRegEx.test(textInput) ||
        fastPairRegEx.test(textInput) || btDeviceRegEx.test(textInput);
    return isRelatedToBluetooth;
  }
}

customElements.define(FeedbackFlowElement.is, FeedbackFlowElement);
