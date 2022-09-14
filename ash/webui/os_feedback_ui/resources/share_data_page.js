// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './os_feedback_shared_css.js';
import './file_attachment.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FEEDBACK_LEGAL_HELP_URL, FEEDBACK_PRIVACY_POLICY_URL, FEEDBACK_TERMS_OF_SERVICE_URL} from './feedback_constants.js';
import {FeedbackFlowState} from './feedback_flow.js';
import {AttachedFile, FeedbackAppPreSubmitAction, FeedbackContext, FeedbackServiceProviderInterface, Report} from './feedback_types.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';

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
      shouldShowBluetoothCheckbox:
          {type: Boolean, readOnly: false, notify: true},
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
     * @type {boolean}
     */
    this.shouldShowBluetoothCheckbox;

    /**
     * @type {string}
     * @protected
     */
    this.sysInfoCheckboxLabel_;

    /**
     * @type {string}
     * @protected
     */
    this.bluetoothLogsCheckboxLabel_;

    /**
     * @type {string}
     * @protected
     */
    this.privacyNote_;

    /** @private {!FeedbackServiceProviderInterface} */
    this.feedbackServiceProvider_ = getFeedbackServiceProvider();
  }

  ready() {
    super.ready();
    this.setPrivacyNote_();
    this.setSysInfoCheckboxLabelAndAttributes_();
    this.setBluetoothLogsCheckboxLabelAndAttributes_();
    // Set the aria description works the best for screen reader.
    // It reads the description when the checkbox is focused, and when it is
    // checked and unchecked.
    this.$.screenshotCheckbox.ariaDescription =
        this.i18n('attachScreenshotCheckboxAriaLabel');
    this.$.imageButton.ariaLabel = this.i18n(
        'previewImageAriaLabel', this.$.screenshotCheckLabel.textContent);
    // The default role is combobox. Set it in JS to avoid the JSCompiler
    // error not recognizing the role property.
    this.$.userEmailDropDown.role = 'listbox';

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
  handleScreenshotClick_() {
    this.$.screenshotDialog.showModal();
    this.feedbackServiceProvider_.recordPreSubmitAction(
        FeedbackAppPreSubmitAction.kViewedScreenshot);
  }

  /** @protected */
  handleScreenshotDialogCloseClick_() {
    this.$.screenshotDialog.close();
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
  handleOpenMetricsDialog_(e) {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    this.feedbackServiceProvider_.openMetricsDialog();
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleOpenSystemInfoDialog_(e) {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    this.feedbackServiceProvider_.openSystemInfoDialog();
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleOpenBluetoothLogsInfoDialog_(e) {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    this.feedbackServiceProvider_.openBluetoothLogsInfoDialog();
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
      detail: {currentState: FeedbackFlowState.SHARE_DATA},
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
        detail: {currentState: FeedbackFlowState.SHARE_DATA, report: report},
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
        url: this.getElement_('#pageUrlText').textContent.trim(),
      };
    }

    if (this.feedbackContext.extraDiagnostics &&
        this.getElement_('#sysInfoCheckbox').checked) {
      report.feedbackContext.extraDiagnostics =
          this.feedbackContext.extraDiagnostics;
    }

    if (this.feedbackContext.categoryTag) {
      report.feedbackContext.categoryTag = this.feedbackContext.categoryTag;
    }

    if (!this.getElement_('#bluetoothCheckboxContainer').hidden &&
        this.getElement_('#bluetoothLogsCheckbox').checked) {
      report.feedbackContext.categoryTag = 'BluetoothReportWithLogs';
      report.sendBluetoothLogs = true;
    } else {
      report.sendBluetoothLogs = false;
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
    // Setting href causes <a> tag to display as link.
    sysInfoLink.setAttribute('href', '#');
    sysInfoLink.addEventListener('click', (e) => {
      this.handleOpenSystemInfoDialog_(e);
      this.feedbackServiceProvider_.recordPreSubmitAction(
          FeedbackAppPreSubmitAction.kViewedSystemAndAppInfo);
    });

    const histogramsLink = this.shadowRoot.querySelector('#histogramsLink');
    histogramsLink.setAttribute('href', '#');
    histogramsLink.addEventListener('click', (e) => {
      this.handleOpenMetricsDialog_(e);
      this.feedbackServiceProvider_.recordPreSubmitAction(
          FeedbackAppPreSubmitAction.kViewedMetrics);
    });
  }

  /** @private */
  setBluetoothLogsCheckboxLabelAndAttributes_() {
    this.bluetoothLogsCheckboxLabel_ =
        this.i18nAdvanced('bluetoothLogsInfo', {attrs: ['id']});

    const bluetoothLogsLink =
        this.shadowRoot.querySelector('#bluetoothLogsInfoLink');
    // Setting href causes <a> tag to display as link.
    bluetoothLogsLink.setAttribute('href', '#');
    bluetoothLogsLink.addEventListener(
        'click', (e) => void this.handleOpenBluetoothLogsInfoDialog_(e));
  }
}

customElements.define(ShareDataPageElement.is, ShareDataPageElement);
