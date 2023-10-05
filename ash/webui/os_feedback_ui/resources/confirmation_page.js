// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeedbackFlowState} from './feedback_flow.js';
import {FeedbackAppPostSubmitAction, FeedbackServiceProviderInterface, SendReportStatus} from './feedback_types.js';
import {showScrollingEffects} from './feedback_utils.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'confirmation-page' is the last step of the feedback tool.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ConfirmationPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

export class ConfirmationPageElement extends ConfirmationPageElementBase {
  static get is() {
    return 'confirmation-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      sendReportStatus: {type: SendReportStatus, readOnly: false, notify: true},
      isUserLoggedIn: {type: Boolean, readOnly: false, notify: true},
    };
  }

  constructor() {
    super();

    /**
     * The status of sending the report.
     * @type {?SendReportStatus}
     */
    this.sendReportStatus;

    /** @private {!FeedbackServiceProviderInterface} */
    this.feedbackServiceProvider_ = getFeedbackServiceProvider();
    /**
     * Whether this is the first action taken by the user after sending
     * feedback.
     * @type {boolean}
     */
    this.isFirstAction = true;

    /**
     * Whether the user has logged in (not on oobe or on the login screen).
     * @type {boolean}
     */
    this.isUserLoggedIn;
  }

  /** @override */
  ready() {
    super.ready();
    window.addEventListener('beforeunload', event => {
      this.handleEmitMetrics_(FeedbackAppPostSubmitAction.kCloseFeedbackApp);
    });
  }

  /**
   * The page shows different information when the device is offline.
   * @returns {boolean}
   * @private
   */
  isOffline_() {
    return this.sendReportStatus === SendReportStatus.kDelayed;
  }

  /**
   * Hide the community link when offline or the user is not logged in.
   * @returns {boolean}
   * @protected
   */
  hideCommunityLink_() {
    return this.isOffline_() || !this.isUserLoggedIn;
  }

  /**
   * @returns {string}
   * @protected
   */
  getTitle_() {
    if (this.isOffline_()) {
      return this.i18n('confirmationTitleOffline');
    }
    return this.i18n('confirmationTitleOnline');
  }

  /**
   * @returns {string}
   * @protected
   */
  getMessage_() {
    if (this.isOffline_()) {
      return this.i18n('thankYouNoteOffline');
    }
    return this.i18n('thankYouNoteOnline');
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleBackButtonClicked_(e) {
    e.stopPropagation();

    this.dispatchEvent(new CustomEvent('go-back-click', {
      composed: true,
      bubbles: true,
      detail: {currentState: FeedbackFlowState.CONFIRMATION},
    }));
    this.handleEmitMetrics_(FeedbackAppPostSubmitAction.kSendNewReport);
  }

  /**
   * Close the app when user clicks the done button.
   * @protected
   */
  handleDoneButtonClicked_() {
    this.handleEmitMetrics_(FeedbackAppPostSubmitAction.kClickDoneButton);
    window.close();
  }

  /**
   * Open links, including SWA app link and web link.
   * @param {!Event} e
   * @protected
   */
  handleLinkClicked_(e) {
    e.stopPropagation();

    switch (e.currentTarget.id) {
      case 'diagnostics':
        this.feedbackServiceProvider_.openDiagnosticsApp();
        this.handleEmitMetrics_(
            FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);
        break;
      case 'explore':
        this.feedbackServiceProvider_.openExploreApp();
        this.handleEmitMetrics_(FeedbackAppPostSubmitAction.kOpenExploreApp);
        break;
      case 'chromebookCommunity':
        // If app locale is not available, default to en.
        window.open(
            `https://support.google.com/chromebook/?hl=${
                this.i18n('language') || 'en'}#topic=3399709`,
            '_blank');
        this.handleEmitMetrics_(
            FeedbackAppPostSubmitAction.kOpenChromebookCommunity);
        break;
      default:
        console.warn('unexpected caller id: ', e.currentTarget.id);
    }
  }

  handleEmitMetrics_(action) {
    if (this.isFirstAction) {
      this.isFirstAction = false;
      this.feedbackServiceProvider_.recordPostSubmitAction(action);
    }
  }

  focusPageTitle() {
    this.shadowRoot.querySelector('#pageTitle').focus();
  }

  /**
   * @param {!Event} event
   * @protected
   */
  onContainerScroll_(event) {
    showScrollingEffects(event, this);
  }
}

customElements.define(ConfirmationPageElement.is, ConfirmationPageElement);
