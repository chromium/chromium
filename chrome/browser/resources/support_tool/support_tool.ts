// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './data_collectors.js';
import './issue_details.js';
import './spinner_page.js';
import './pii_selection.js';
import './data_export_done.js';
import './support_tool_shared.css.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {BrowserProxy, PiiDataItem, StartDataCollectionResult} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {DataCollectorsElement} from './data_collectors.js';
import type {DataExportDoneElement} from './data_export_done.js';
import type {IssueDetailsElement} from './issue_details.js';
import type {PiiSelectionElement} from './pii_selection.js';
import type {SpinnerPageElement} from './spinner_page.js';
import {getTemplate} from './support_tool.html.js';

export enum SupportToolPageIndex {
  ISSUE_DETAILS,
  DATA_COLLECTOR_SELECTION,
  SPINNER,
  PII_SELECTION,
  EXPORT_SPINNER,
  DATA_EXPORT_DONE,
}

export interface DataExportResult {
  success: boolean;
  path: string;
  error: string;
}

export interface SupportToolElement {
  $: {
    issueDetails: IssueDetailsElement,
    dataCollectors: DataCollectorsElement,
    spinnerPage: SpinnerPageElement,
    piiSelection: PiiSelectionElement,
    exportSpinner: SpinnerPageElement,
    dataExportDone: DataExportDoneElement,
    errorMessageToast: CrToastElement,
  };
}

const SupportToolElementBase = WebUiListenerMixin(PolymerElement);

export class SupportToolElement extends SupportToolElementBase {
  static get is() {
    return 'support-tool';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedPage_: {
        type: SupportToolPageIndex,
        value: SupportToolPageIndex.ISSUE_DETAILS,
        observer: 'onSelectedPageChange_',
      },
      supportToolPageIndex_: {
        readonly: true,
        type: Object,
        value: SupportToolPageIndex,
      },
      // The error message shown in errorMessageToast element when it's shown.
      errorMessage_: {
        type: String,
        value: '',
      },
    };
  }

  private errorMessage_: string;
  private selectedPage_: SupportToolPageIndex;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'screenshot-received', this.onScreenshotReceived_.bind(this));
    this.addWebUiListener(
        'data-collection-completed',
        this.onDataCollectionCompleted_.bind(this));
    this.addWebUiListener(
        'data-collection-cancelled',
        this.onDataCollectionCancelled_.bind(this));
    this.addWebUiListener(
        'support-data-export-started', this.onDataExportStarted_.bind(this));
    this.addWebUiListener(
        'data-export-completed', this.onDataExportCompleted_.bind(this));
  }

  private onScreenshotReceived_(screenshotBase64: string) {
    if (screenshotBase64 !== 'CANCELED') {
      // Only continues if the user didn't cancel the screenshot.
      this.$.dataCollectors.setScreenshotData(screenshotBase64);
    }
  }

  private onDataExportStarted_() {
    this.selectedPage_ = SupportToolPageIndex.EXPORT_SPINNER;
  }

  private onDataCollectionCompleted_(piiItems: PiiDataItem[]) {
    this.$.piiSelection.updateDetectedPiiItems(piiItems);
    this.selectedPage_ = SupportToolPageIndex.PII_SELECTION;
  }

  private onDataCollectionCancelled_() {
    // Change the selected page into issue details page so they user can restart
    // data collection if they want.
    this.selectedPage_ = SupportToolPageIndex.ISSUE_DETAILS;
  }

  private displayError_(errorMessage: string) {
    this.errorMessage_ = errorMessage;
    this.$.errorMessageToast.show();
  }

  private onDataExportCompleted_(result: DataExportResult) {
    if (result.success) {
      // Show the exported data path to user in data export page if the data
      // export is successful.
      this.$.dataExportDone.setPath(result.path);
      this.selectedPage_ = SupportToolPageIndex.DATA_EXPORT_DONE;
    } else {
      // Show a toast with error message and turn back to the PII selection page
      // so the user can retry exporting their data.
      this.selectedPage_ = SupportToolPageIndex.PII_SELECTION;
      this.displayError_(result.error);
    }
  }

  private onDataCollectionStart_(result: StartDataCollectionResult) {
    if (result.success) {
      this.selectedPage_ = SupportToolPageIndex.SPINNER;
    } else {
      this.displayError_(result.errorMessage);
    }
  }

  private onErrorMessageToastCloseClicked_() {
    this.$.errorMessageToast.hide();
  }

  private onContinueClick_() {
    // If we are currently on data collectors selection page, send signal to
    // start data collection.
    if (this.selectedPage_ === SupportToolPageIndex.DATA_COLLECTOR_SELECTION) {
      this.browserProxy_
          .startDataCollection(
              this.$.issueDetails.getIssueDetails(),
              this.$.dataCollectors.getDataCollectors(),
              this.$.dataCollectors.getEditedScreenshotBase64())
          .then(this.onDataCollectionStart_.bind(this));
    } else {
      this.selectedPage_ = this.selectedPage_ + 1;
    }
  }

  private onBackClick_() {
    this.selectedPage_ = this.selectedPage_ - 1;
  }

  private shouldHideBackButton_(): boolean {
    // Back button will only be shown on data collectors selection page.
    return this.selectedPage_ !== SupportToolPageIndex.DATA_COLLECTOR_SELECTION;
  }

  private shouldHideContinueButtonContainer_(): boolean {
    // Continue button container will only be shown in issue details page and
    // data collectors selection page.
    return this.selectedPage_ >= SupportToolPageIndex.SPINNER;
  }

  private onSelectedPageChange_() {
    // On every selected page change, the focus will be moved to each page's
    // header to ensure a smooth experience in terms of accessibility.
    switch (this.selectedPage_) {
      case SupportToolPageIndex.ISSUE_DETAILS:
        this.$.issueDetails.ensureFocusOnPageHeader();
        break;
      case SupportToolPageIndex.DATA_COLLECTOR_SELECTION:
        this.$.dataCollectors.ensureFocusOnPageHeader();
        break;
      case SupportToolPageIndex.SPINNER:
        this.$.spinnerPage.ensureFocusOnPageHeader();
        break;
      case SupportToolPageIndex.PII_SELECTION:
        this.$.piiSelection.ensureFocusOnPageHeader();
        break;
      case SupportToolPageIndex.EXPORT_SPINNER:
        this.$.exportSpinner.ensureFocusOnPageHeader();
        break;
      case SupportToolPageIndex.DATA_EXPORT_DONE:
        this.$.dataExportDone.ensureFocusOnPageHeader();
        break;
      default:
        break;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'support-tool': SupportToolElement;
  }
}

customElements.define(SupportToolElement.is, SupportToolElement);
