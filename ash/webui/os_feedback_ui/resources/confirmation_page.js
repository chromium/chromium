// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeedbackFlowState} from './feedback_flow.js';
import {FeedbackServiceProviderInterface, SendReportStatus} from './feedback_types.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'confirmation-page' is the last step of the feedback tool.
 */
export class ConfirmationPageElement extends PolymerElement {
  static get is() {
    return 'confirmation-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      sendReportStatus: {type: SendReportStatus, readOnly: false, notify: true},
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
  }

  /**
   * The page shows different information when the device is offline.
   * @returns {boolean}
   * @protected
   */
  isOffline_() {
    return this.sendReportStatus === SendReportStatus.kDelayed;
  }

  /**
   * @returns {string}
   * @protected
   */
  getTitle_() {
    // TODO(xiangdongkong): Localize the strings.
    if (this.isOffline_()) {
      return 'You are offline now. Feedback will be sent later.';
    }
    return 'Thanks for your feedback';
  }

  /**
   * @returns {string}
   * @protected
   */
  getMessage_() {
    // TODO(xiangdongkong): Localize the strings.
    if (this.isOffline_()) {
      return 'Thanks for the feedback. Your feedback helps improve Chrome OS ' +
          'and will be reviewed by the Chrome OS team. Because of the number ' +
          ' of reports submitted, you won’t receive a direct reply. ';
    }
    return 'Your feedback helps improve ChromeOS and will be reviewed by ' +
        'our team. Because of the large number of reports, we won\’t be able ' +
        ' to send a reply.';
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
      detail: {currentState: FeedbackFlowState.CONFIRMATION}
    }));
  }

  /**
   * Close the app when user clicks the done button.
   * @protected
   */
  handleDoneButtonClicked_() {
    window.close();
  }

  /**
   * Open links, including SWA app link and web link.
   * @param {!Event} e
   * @protected
   */
  handleLinkClicked_(e) {
    e.stopPropagation();

    switch (e.target.id) {
      case 'diagnostics':
        this.feedbackServiceProvider_.openDiagnosticsApp();
        break;
      default:
        console.warn('unexpected caller id: ', e.target.id);
    }
  }
}

customElements.define(ConfirmationPageElement.is, ConfirmationPageElement);
