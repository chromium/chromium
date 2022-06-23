// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './os_feedback_shared_css.js';
import './file_attachment.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FEEDBACK_LEGAL_HELP_URL, FEEDBACK_PRIVACY_POLICY_URL, FEEDBACK_TERMS_OF_SERVICE_URL} from './feedback_constants.js';
import {FeedbackFlowState} from './feedback_flow.js';
import {AttachedFile, FeedbackContext, Report} from './feedback_types.js';

/**
 * @fileoverview
 * 'share-data-page' is the second page of the feedback tool. It allows users to
 * choose what data to send with the feedback report.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ShareDataPageElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ShareDataPageElement extends ShareDataPageElementBase {
  static get is() {
    return 'share-data-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      feedbackContext: {type: FeedbackContext, readOnly: false, notify: true},
      screenshotUrl: {type: String, readOnly: false, notify: true},
    };
  }

  constructor() {
    super();

    /**
     * @type {!FeedbackContext}
     */
    this.feedbackContext;

    /**
     * @type {string}
     */
    this.screenshotUrl;

    /**
     * @type {string}
     * @protected
     */
    this.sysInfoCheckboxLabel_;

    /**
     * @type {string}
     * @protected
     */
    this.privacyNote_;
  }

  ready() {
    super.ready();
    this.setPrivacyNote_();
    this.setSysInfoCheckboxLabelAndAttributes_();

    // Set up event listener for email change to retarget |this| to be the
    // ShareDataPageElement's context.
    this.$.userEmailDropDown.addEventListener(
        'change', this.handleUserEmailDropDownChanged_.bind(this));
  }

  /**
   * @return {boolean}
   * @protected
   */
  hasEmail_() {
    return (this.feedbackContext !== null && !!this.feedbackContext.email);
  }

  /**
   * @return {boolean}
   * @protected
   */
  hasScreenshot_() {
    return !!this.screenshotUrl;
  }

  /** @protected */
  handleUserEmailDropDownChanged_() {
    const email = this.$.userEmailDropDown.value;
    const consentCheckbox = this.$.userConsentCheckbox;

    // Update UI and state of #userConsentCheckbox base on if report will be
    // anonymous.
    if (email === '') {
      consentCheckbox.disabled = true;
      consentCheckbox.checked = false;
      this.$.userConsentLabel.classList.add('disabled-input-text');
    } else {
      consentCheckbox.disabled = false;
      this.$.userConsentLabel.classList.remove('disabled-input-text');
    }
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
      detail: {currentState: FeedbackFlowState.SHARE_DATA}
    }));
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleSendButtonClicked_(e) {
    this.getElement_('#buttonSend').disabled = true;

    e.stopPropagation();

    this.createReport_().then(report => {
      this.dispatchEvent(new CustomEvent('continue-click', {
        composed: true,
        bubbles: true,
        detail: {currentState: FeedbackFlowState.SHARE_DATA, report: report}
      }));
    });
  }

  /**
   * @param {string} selector
   * @return {Element}
   * @private
   */
  getElement_(selector) {
    return this.shadowRoot.querySelector(selector);
  }

  /**
   * @return {!Promise<!Report>}
   * @private
   */
  async createReport_() {
    /* @type {!Report} */
    const report = /** @type {!Report} */ ({
      feedbackContext: {},
      description: null,
      includeSystemLogsAndHistograms:
          this.getElement_('#sysInfoCheckbox').checked,
      includeScreenshot: this.getElement_('#screenshotCheckbox').checked &&
          !!this.getElement_('#screenshotImage').src,
      contactUserConsentGranted:
          this.getElement_('#userConsentCheckbox').checked,
    });

    report.attachedFile =
        await this.getElement_('file-attachment').getAttachedFile();

    const email = this.getElement_('#userEmailDropDown').value;
    if (email) {
      report.feedbackContext.email = email;
    }

    // Ensure consent granted is false when email not provided.
    if (!email) {
      report.contactUserConsentGranted = false;
    }

    if (this.getElement_('#pageUrlCheckbox').checked) {
      report.feedbackContext.pageUrl = {
        url: this.getElement_('#pageUrlText').value
      };
    }

    if (this.feedbackContext.extraDiagnostics &&
        this.getElement_('#sysInfoCheckbox').checked) {
      report.feedbackContext.extraDiagnostics =
          this.feedbackContext.extraDiagnostics;
    }

    return report;
  }

  /**
   * When starting a new report, the send report button should be
   * re-enabled.
   */
  reEnableSendReportButton() {
    this.getElement_('#buttonSend').disabled = false;
  }

  /**
   * Make the link clickable and open it in a new window
   * @param {!string} linkSelector
   * @param {!string} linkUrl
   * @private
   */
  openLinkInNewWindow_(linkSelector, linkUrl) {
    const linkElement = this.shadowRoot.querySelector(linkSelector);
    linkElement.setAttribute('href', linkUrl);
    linkElement.setAttribute('target', '_blank');
  }

  /** @private */
  setPrivacyNote_() {
    this.privacyNote_ = this.i18nAdvanced('privacyNote', {attrs: ['id']});

    this.openLinkInNewWindow_('#legalHelpPageUrl', FEEDBACK_LEGAL_HELP_URL);
    this.openLinkInNewWindow_('#privacyPolicyUrl', FEEDBACK_PRIVACY_POLICY_URL);
    this.openLinkInNewWindow_(
        '#termsOfServiceUrl', FEEDBACK_TERMS_OF_SERVICE_URL);
  }

  /** @private */
  setSysInfoCheckboxLabelAndAttributes_() {
    this.sysInfoCheckboxLabel_ = this.i18nAdvanced(
        'includeSystemInfoAndMetricsCheckboxLabel', {attrs: ['id']});

    const sysInfoLink = this.shadowRoot.querySelector('#sysInfoLink');
    sysInfoLink.setAttribute('href', '#');
    const histogramsLink = this.shadowRoot.querySelector('#histogramsLink');
    histogramsLink.setAttribute('href', '#');
  }
}

customElements.define(ShareDataPageElement.is, ShareDataPageElement);
