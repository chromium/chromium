// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import './file_attachment.js';
import './os_feedback_shared.css.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FEEDBACK_LEGAL_HELP_URL, FEEDBACK_PRIVACY_POLICY_URL, FEEDBACK_TERMS_OF_SERVICE_URL} from './feedback_constants.js';
import {FeedbackFlowButtonClickEvent, FeedbackFlowState} from './feedback_flow.js';
import {showScrollingEffects} from './feedback_utils.js';
import {FileAttachmentElement} from './file_attachment.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';
import {FeedbackAppPreSubmitAction, FeedbackContext, FeedbackServiceProviderInterface, Report} from './os_feedback_ui.mojom-webui.js';
import {getTemplate} from './share_data_page.html.js';

/**
 * @fileoverview
 * 'share-data-page' is the second page of the feedback tool. It allows users to
 * choose what data to send with the feedback report.
 */

const ShareDataPageElementBase = I18nMixin(PolymerElement);

export class ShareDataPageElement extends ShareDataPageElementBase {
  static get is() {
    return 'share-data-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      feedbackContext: {
        type: Object,
        readOnly: false,
        notify: true,
        observer: ShareDataPageElement.prototype.onFeedbackContextChanged,
      },

      screenshotUrl: {type: String, readOnly: false, notify: true},
      shouldShowBluetoothCheckbox:
          {type: Boolean, readOnly: false, notify: true},
      shouldShowLinkCrossDeviceDogfoodFeedbackCheckbox:
          {type: Boolean, readOnly: false, notify: true},
      shouldShowAssistantCheckbox:
          {type: Boolean, readOnly: false, notify: true},
      shouldShowAutofillCheckbox:
          {type: Boolean, readOnly: false, notify: true},
    };
  }

  feedbackContext: FeedbackContext;
  screenshotUrl: string;
  shouldShowBluetoothCheckbox: boolean;
  shouldShowWifiDebugLogsCheckbox: boolean;
  shouldShowLinkCrossDeviceDogfoodFeedbackCheckbox: boolean;
  shouldShowAssistantCheckbox: boolean;
  shouldShowAutofillCheckbox: boolean;
  private feedbackServiceProvider: FeedbackServiceProviderInterface;

  constructor() {
    super();

    this.feedbackServiceProvider = getFeedbackServiceProvider();
  }

  override ready() {
    super.ready();
    this.setLinksInPrivacyNote();
    this.setSysInfoCheckboxAttributes();
    this.setAssistantLogsAttributes();
    this.setBluetoothLogsAttributes();
    this.setWifiDebugLogsAttributes();
    this.setLinkCrossDeviceDogfoodFeedbackAttributes();
    this.setAutofillAttributes();
    // Set the aria description works the best for screen reader.
    // It reads the description when the checkbox is focused, and when it is
    // checked and unchecked.
    strictQuery('#screenshotCheckbox', this.shadowRoot, CrCheckboxElement)
        .ariaDescription = this.i18n('attachScreenshotCheckboxAriaLabel');
    strictQuery('#imageButton', this.shadowRoot, HTMLButtonElement).ariaLabel =
        this.i18n(
            'previewImageAriaLabel',
            strictQuery('#screenshotCheckLabel', this.shadowRoot, HTMLElement)
                    .textContent ??
                '');

    // Set up event listener for email change to retarget |this| to be the
    // ShareDataPageElement's context.
    strictQuery('#userEmailDropDown', this.shadowRoot, HTMLSelectElement)
        .addEventListener(
            'change', this.handleUserEmailDropDownChanged.bind(this));
  }

  hasEmail(): boolean {
    return (this.feedbackContext !== null && !!this.feedbackContext.email);
  }

  /**
   * If feedback app has been requested from settings search, we do not need to
   * collect system info and metrics data by default.
   */
  protected checkSysInfoAndMetrics(): boolean {
    if (!this.feedbackContext) {
      return true;
    }
    return !this.feedbackContext.fromSettingsSearch;
  }

  shouldShowPerformanceTraceCheckbox(): boolean {
    return (
        this.feedbackContext !== null && this.feedbackContext.traceId !== 0);
  }

  /** Focus on the screenshot checkbox when entering the page. */
  focusScreenshotCheckbox() {
    strictQuery('#screenshotCheckbox', this.shadowRoot, CrCheckboxElement)
        .focus();
  }

  hasScreenshot(): boolean {
    return !!this.screenshotUrl;
  }

  protected handleScreenshotClick(): void {
    strictQuery('#screenshotDialog', this.shadowRoot, HTMLDialogElement)
        .showModal();
    this.feedbackServiceProvider.recordPreSubmitAction(
        FeedbackAppPreSubmitAction.kViewedScreenshot);
  }

  protected handleScreenshotDialogCloseClick(): void {
    strictQuery('#screenshotDialog', this.shadowRoot, HTMLDialogElement)
        .close();
  }

  protected handleUserEmailDropDownChanged(): void {
    const email =
        strictQuery('#userEmailDropDown', this.shadowRoot, HTMLSelectElement)
            .value;
    const consentCheckbox =
        strictQuery('#userConsentCheckbox', this.shadowRoot, CrCheckboxElement);

    // Update UI and state of #userConsentCheckbox base on if report will be
    // anonymous.
    if (email === '') {
      consentCheckbox.disabled = true;
      consentCheckbox.checked = false;
      strictQuery('#userConsentLabel', this.shadowRoot, HTMLElement)
          .classList.add('disabled-input-text');
    } else {
      consentCheckbox.disabled = false;
      strictQuery('#userConsentLabel', this.shadowRoot, HTMLElement)
          .classList.remove('disabled-input-text');
    }
  }

  protected handleOpenMetricsDialog(e: Event): void {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    this.feedbackServiceProvider.openMetricsDialog();
  }

  protected handleOpenSystemInfoDialog(e: Event): void {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    this.feedbackServiceProvider.openSystemInfoDialog();
  }

  protected handleOpenAutofillMetadataDialog(e: Event): void {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    this.feedbackServiceProvider.openAutofillDialog(
        this.feedbackContext.autofillMetadata || '');
  }

  protected handleOpenBluetoothLogsInfoDialog(e: Event): void {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    strictQuery('#bluetoothDialog', this.shadowRoot, CrDialogElement)
        .showModal();
    strictQuery('#bluetoothDialogDoneButton', this.shadowRoot, CrButtonElement)
        .focus();
  }

  protected handleCloseBluetoothDialogClicked(): void {
    strictQuery('#bluetoothDialog', this.shadowRoot, CrDialogElement).close();
  }

  protected handleOpenWifiDebugLogsInfoDialog(e: Event): void {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    strictQuery('#wifiDebugLogsDialog', this.shadowRoot, CrDialogElement)
        .showModal();
    strictQuery(
        '#wifiDebugLogsDialogDoneButton', this.shadowRoot, CrButtonElement)
        .focus();
  }

  protected handleCloseWifiDebugLogsDialogClicked(): void {
    strictQuery('#wifiDebugLogsDialog', this.shadowRoot, CrDialogElement)
        .close();
  }

  protected handleOpenLinkCrossDeviceDogfoodFeedbackInfoDialog(e: Event): void {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    strictQuery(
        '#linkCrossDeviceDogfoodFeedbackDialog', this.shadowRoot,
        CrDialogElement)
        .showModal();
    strictQuery(
        '#linkCrossDeviceDogfoodFeedbackDialogDoneButton', this.shadowRoot,
        CrButtonElement)
        .focus();
  }

  protected handleCloseLinkCrossDeviceDogfoodFeedbackDialogClicked(): void {
    strictQuery(
        '#linkCrossDeviceDogfoodFeedbackDialog', this.shadowRoot,
        CrDialogElement)
        .close();
  }

  protected handleOpenAssistantLogsDialog(e: Event): void {
    // The default behavior of clicking on an anchor tag
    // with href="#" is a scroll to the top of the page.
    // This link opens a dialog, so we want to prevent
    // this default behavior.
    e.preventDefault();

    strictQuery('#assistantDialog', this.shadowRoot, CrDialogElement)
        .showModal();
    strictQuery('#assistantDialogDoneButton', this.shadowRoot, CrButtonElement)
        .focus();
  }

  protected handleCloseAssistantDialogClicked(): void {
    strictQuery('#assistantDialog', this.shadowRoot, CrDialogElement).close();
  }

  protected handleBackButtonClicked(e: Event): void {
    e.stopPropagation();

    this.dispatchEvent(new CustomEvent('go-back-click', {
      composed: true,
      bubbles: true,
      detail: {currentState: FeedbackFlowState.SHARE_DATA},
    }));
  }

  protected handleSendButtonClicked(e: Event): void {
    strictQuery('#buttonSend', this.shadowRoot, CrButtonElement).disabled =
        true;

    e.stopPropagation();

    this.createReport().then(report => {
      this.dispatchEvent(new CustomEvent('continue-click', {
        composed: true,
        bubbles: true,
        detail: {currentState: FeedbackFlowState.SHARE_DATA, report: report},
      }));
    });
  }

  private async createReport(): Promise<Report> {
    const report: Report = ({
      feedbackContext: {
        assistantDebugInfoAllowed: false,
        fromSettingsSearch: false,
        isInternalAccount: false,
        wifiDebugLogsAllowed: false,
        traceId: 0,
        pageUrl: null,
        fromAssistant: false,
        fromAutofill: false,
        autofillMetadata: '{}',
        hasLinkedCrossDevicePhone: false,
        categoryTag: '',
        email: '',
        extraDiagnostics: '',
      },
      description: {data: []},
      attachedFile: null,
      sendBluetoothLogs: false,
      sendWifiDebugLogs: false,
      includeAutofillMetadata: false,
      includeSystemLogsAndHistograms:
          strictQuery('#sysInfoCheckbox', this.shadowRoot, CrCheckboxElement)
              .checked,
      includeScreenshot:
          strictQuery('#screenshotCheckbox', this.shadowRoot, CrCheckboxElement)
              .checked &&
          !!strictQuery('#screenshotImage', this.shadowRoot, HTMLImageElement)
                .src,
      contactUserConsentGranted:
          strictQuery(
              '#userConsentCheckbox', this.shadowRoot, CrCheckboxElement)
              .checked,
    });
    const attachedFile =
        await strictQuery(
            'file-attachment', this.shadowRoot, FileAttachmentElement)
            .getAttachedFile();
    if (attachedFile) {
      report.attachedFile = attachedFile;
    }

    const email =
        strictQuery('#userEmailDropDown', this.shadowRoot, HTMLSelectElement)
            .value;
    if (email) {
      report.feedbackContext.email = email;
    }

    // Ensure consent granted is false when email not provided.
    if (!email) {
      report.contactUserConsentGranted = false;
    }

    if (strictQuery('#pageUrlCheckbox', this.shadowRoot, CrCheckboxElement)
            .checked) {
      report.feedbackContext.pageUrl = {
        url: strictQuery('#pageUrlText', this.shadowRoot, HTMLElement)
                 .textContent!.trim(),
      };
    }

    if (this.feedbackContext.extraDiagnostics &&
        strictQuery('#sysInfoCheckbox', this.shadowRoot, CrCheckboxElement)
            .checked) {
      report.feedbackContext.extraDiagnostics =
          this.feedbackContext.extraDiagnostics;
    }

    if (this.feedbackContext.categoryTag) {
      report.feedbackContext.categoryTag = this.feedbackContext.categoryTag;
    }

    const isLinkCrossDeviceIssue =
        !strictQuery(
             '#linkCrossDeviceDogfoodFeedbackCheckboxContainer',
             this.shadowRoot, HTMLElement)
             .hidden &&
        strictQuery(
            '#linkCrossDeviceDogfoodFeedbackCheckbox', this.shadowRoot,
            CrCheckboxElement)
            .checked;

    if (!strictQuery(
             '#bluetoothCheckboxContainer', this.shadowRoot, HTMLElement)
             .hidden &&
        strictQuery(
            '#bluetoothLogsCheckbox', this.shadowRoot, CrCheckboxElement)
            .checked) {
      report.feedbackContext.categoryTag = isLinkCrossDeviceIssue ?
          'linkCrossDeviceDogfoodFeedbackWithBluetoothLogs' :
          'BluetoothReportWithLogs';
      report.sendBluetoothLogs = true;
    } else {
      if (isLinkCrossDeviceIssue) {
        report.feedbackContext.categoryTag =
            'linkCrossDeviceDogfoodFeedbackWithoutBluetoothLogs';
      }
      report.sendBluetoothLogs = false;
    }

    if (this.feedbackContext.fromAutofill &&
        !strictQuery('#autofillCheckboxContainer', this.shadowRoot, HTMLElement)
             .hidden &&
        strictQuery('#autofillCheckbox', this.shadowRoot, CrCheckboxElement)
            .checked) {
      report.includeAutofillMetadata = true;
      report.feedbackContext.autofillMetadata =
          this.feedbackContext.autofillMetadata;
    } else {
      report.includeAutofillMetadata = false;
      report.feedbackContext.autofillMetadata = '';
    }

    report.sendWifiDebugLogs = this.shouldShowWifiDebugLogsCheckbox &&
        strictQuery(
            '#wifiDebugLogsCheckbox', this.shadowRoot, CrCheckboxElement)
            .checked;

    if (strictQuery(
            '#performanceTraceCheckbox', this.shadowRoot, CrCheckboxElement)
            .checked) {
      report.feedbackContext.traceId = this.feedbackContext.traceId;
    } else {
      report.feedbackContext.traceId = 0;
    }

    report.feedbackContext.fromAssistant = this.feedbackContext.fromAssistant;

    report.feedbackContext.assistantDebugInfoAllowed =
        this.feedbackContext.fromAssistant &&
        !strictQuery('#assistantLogsContainer', this.shadowRoot, HTMLElement)
             .hidden &&
        strictQuery(
            '#assiatantLogsCheckbox', this.shadowRoot, CrCheckboxElement)
            .checked;

    return report;
  }

  /**
   * When starting a new report, the send report button should be
   * re-enabled.
   */
  reEnableSendReportButton(): void {
    strictQuery('#buttonSend', this.shadowRoot, CrButtonElement).disabled =
        false;
  }

  /**
   * Make the link clickable and open it in a new window
   */
  private openLinkInNewWindow(linkSelector: string, linkUrl: string): void {
    const linkElement = this.shadowRoot!.querySelector(linkSelector);
    if (linkElement) {
      linkElement.setAttribute('href', linkUrl);
      linkElement.setAttribute('target', '_blank');
    }
  }

  /**
   * When the feedback app is launched from OOBE or the login screen, the
   * categoryTag is set to "Login".
   */
  protected isUserLoggedIn(): boolean {
    return this.feedbackContext?.categoryTag !== 'Login';
  }

  protected getAttachFilesLabel(): string {
    return this.isUserLoggedIn() ? this.i18n('attachFilesLabelLoggedIn') :
                                   this.i18n('attachFilesLabelLoggedOut');
  }

  protected getSysInfoCheckboxLabel(): TrustedHTML {
    return this.i18nAdvanced(
        'includeSystemInfoAndMetricsCheckboxLabel', {attrs: ['id']});
  }

  protected getPerformanceTraceCheckboxLabel(): TrustedHTML {
    return this.i18nAdvanced(
        'includePerformanceTraceCheckboxLabel', {attrs: ['id']});
  }

  protected getAssistantLogsCheckboxLabel(): TrustedHTML {
    return this.i18nAdvanced(
        'includeAssistantLogsCheckboxLabel', {attrs: ['id']});
  }

  protected getAutofillCheckboxLabel(): TrustedHTML {
    return this.i18nAdvanced('includeAutofillCheckboxLabel', {attrs: ['id']});
  }

  protected getBluetoothLogsCheckboxLabel(): TrustedHTML {
    return this.i18nAdvanced('bluetoothLogsInfo', {attrs: ['id']});
  }

  protected getWifiDebugLogsCheckboxLabel(): TrustedHTML {
    return this.i18nAdvanced('wifiDebugLogsInfo', {attrs: ['id']});
  }

  protected getLinkCrossDeviceDogfoodFeedbackCheckboxLabel(): TrustedHTML {
    return this.i18nAdvanced(
        'linkCrossDeviceDogfoodFeedbackInfo', {attrs: ['id']});
  }

  protected getPrivacyNote(): TrustedHTML {
    if (this.isUserLoggedIn()) {
      return this.i18nAdvanced('privacyNote', {attrs: ['id']});
    } else {
      return this.i18nAdvanced('privacyNoteLoggedOut', {
        substitutions: [
          FEEDBACK_PRIVACY_POLICY_URL,
          FEEDBACK_TERMS_OF_SERVICE_URL,
          FEEDBACK_LEGAL_HELP_URL,
        ],
      });
    }
  }

  private setLinksInPrivacyNote(): void {
    this.openLinkInNewWindow('#legalHelpPageUrl', FEEDBACK_LEGAL_HELP_URL);
    this.openLinkInNewWindow('#privacyPolicyUrl', FEEDBACK_PRIVACY_POLICY_URL);
    this.openLinkInNewWindow(
        '#termsOfServiceUrl', FEEDBACK_TERMS_OF_SERVICE_URL);
  }

  private setSysInfoCheckboxAttributes(): void {
    const sysInfoLink =
        strictQuery('#sysInfoLink', this.shadowRoot, HTMLAnchorElement);
    // Setting href causes <a> tag to display as link.
    sysInfoLink.setAttribute('href', '#');
    sysInfoLink.addEventListener('click', (e: Event) => {
      this.handleOpenSystemInfoDialog(e);
      this.feedbackServiceProvider.recordPreSubmitAction(
          FeedbackAppPreSubmitAction.kViewedSystemAndAppInfo);
    });

    const histogramsLink =
        strictQuery('#histogramsLink', this.shadowRoot, HTMLAnchorElement);
    histogramsLink.setAttribute('href', '#');
    histogramsLink.addEventListener('click', (e: Event) => {
      this.handleOpenMetricsDialog(e);
      this.feedbackServiceProvider.recordPreSubmitAction(
          FeedbackAppPreSubmitAction.kViewedMetrics);
    });
  }

  private setAutofillAttributes(): void {
    const assistantLogsLink =
        strictQuery('#autofillMetadataUrl', this.shadowRoot, HTMLAnchorElement);
    // Setting href causes <a> tag to display as link.
    assistantLogsLink.setAttribute('href', '#');
    assistantLogsLink.addEventListener('click', (e: Event) => {
      this.handleOpenAutofillMetadataDialog(e);
      this.feedbackServiceProvider.recordPreSubmitAction(
          FeedbackAppPreSubmitAction.kViewedAutofillMetadata);
    });
  }

  private setAssistantLogsAttributes(): void {
    const assistantLogsLink =
        strictQuery('#assistantLogsLink', this.shadowRoot, HTMLAnchorElement);
    // Setting href causes <a> tag to display as link.
    assistantLogsLink.setAttribute('href', '#');
    assistantLogsLink.addEventListener(
        'click', (e: Event) => void this.handleOpenAssistantLogsDialog(e));
  }

  private setBluetoothLogsAttributes(): void {
    const bluetoothLogsLink = strictQuery(
        '#bluetoothLogsInfoLink', this.shadowRoot, HTMLAnchorElement);
    // Setting href causes <a> tag to display as link.
    bluetoothLogsLink.setAttribute('href', '#');
    bluetoothLogsLink.addEventListener(
        'click', (e: Event) => void this.handleOpenBluetoothLogsInfoDialog(e));
  }

  private setWifiDebugLogsAttributes(): void {
    const wifiDebugLogsLink = strictQuery(
        '#wifiDebugLogsInfoLink', this.shadowRoot, HTMLAnchorElement);
    // Setting href causes <a> tag to display as link.
    wifiDebugLogsLink.setAttribute('href', '#');
    wifiDebugLogsLink.addEventListener(
        'click', (e: Event) => void this.handleOpenWifiDebugLogsInfoDialog(e));
  }

  private setLinkCrossDeviceDogfoodFeedbackAttributes(): void {
    const linkCrossDeviceDogfoodFeedbackLink = strictQuery(
        '#linkCrossDeviceDogfoodFeedbackInfoLink', this.shadowRoot,
        HTMLAnchorElement);
    // Setting href causes <a> tag to display as link.
    linkCrossDeviceDogfoodFeedbackLink.setAttribute('href', '#');
    linkCrossDeviceDogfoodFeedbackLink.addEventListener(
        'click',
        (e: Event) =>
            void this.handleOpenLinkCrossDeviceDogfoodFeedbackInfoDialog(e));
  }

  private onFeedbackContextChanged(): void {
    // We can only set up the hyperlink for the performance trace checkbox once
    // we receive the trace id.
    if (this.feedbackContext !== null && this.feedbackContext.traceId !== 0) {
      this.openLinkInNewWindow(
          '#performanceTraceLink',
          `chrome://slow_trace/tracing.zip#${this.feedbackContext.traceId}`);
    }
    // Update the privacy note when the feedback context changed.
    this.setLinksInPrivacyNote();
  }

  protected onContainerScroll(event: Event): void {
    showScrollingEffects(event, this as HTMLElement);
  }
}

declare global {
  interface HTMLElementEventMap {
    'continue-click': FeedbackFlowButtonClickEvent;
    'go-back-click': FeedbackFlowButtonClickEvent;
  }

  interface HTMLElementTagNameMap {
    [ShareDataPageElement.is]: ShareDataPageElement;
  }
}

customElements.define(ShareDataPageElement.is, ShareDataPageElement);
