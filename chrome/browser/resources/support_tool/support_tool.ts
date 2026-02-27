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

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy, PiiDataItem, StartDataCollectionResult} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {DataCollectorsElement} from './data_collectors.js';
import type {DataExportDoneElement} from './data_export_done.js';
import type {IssueDetailsElement} from './issue_details.js';
import type {PiiSelectionElement} from './pii_selection.js';
import type {SpinnerPageElement} from './spinner_page.js';
import {getCss} from './support_tool.css.js';
import {getHtml} from './support_tool.html.js';

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
    continueButton: CrButtonElement,
    continueButtonContainer: HTMLElement,
    issueDetails: IssueDetailsElement,
    dataCollectors: DataCollectorsElement,
    spinnerPage: SpinnerPageElement,
    piiSelection: PiiSelectionElement,
    exportSpinner: SpinnerPageElement,
    dataExportDone: DataExportDoneElement,
    errorMessageToast: CrToastElement,
  };
}

const SupportToolElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class SupportToolElement extends SupportToolElementBase {
  static get is() {
    return 'support-tool';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedPage_: {type: Number},
      errorMessage_: {type: String},
    };
  }

  protected accessor errorMessage_: string = '';
  protected accessor selectedPage_: SupportToolPageIndex =
      SupportToolPageIndex.ISSUE_DETAILS;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('selectedPage_')) {
      this.onSelectedPageChange_();
    }
  }

  protected onDataExportStarted_() {
    this.selectedPage_ = SupportToolPageIndex.EXPORT_SPINNER;
  }

  protected onDataCollectionCompleted_(piiItems: PiiDataItem[]) {
    this.$.piiSelection.updateDetectedPiiItems(piiItems);
    this.selectedPage_ = SupportToolPageIndex.PII_SELECTION;
  }

  protected onDataCollectionCancelled_() {
    // Change the selected page into issue details page so they user can restart
    // data collection if they want.
    this.selectedPage_ = SupportToolPageIndex.ISSUE_DETAILS;
  }

  protected displayError_(errorMessage: string) {
    this.errorMessage_ = errorMessage;
    this.$.errorMessageToast.show();
  }

  protected onDataExportCompleted_(result: DataExportResult) {
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

  protected onDataCollectionStart_(result: StartDataCollectionResult) {
    if (result.success) {
      this.selectedPage_ = SupportToolPageIndex.SPINNER;
    } else {
      this.displayError_(result.errorMessage);
    }
  }

  protected onErrorMessageToastCloseClick_() {
    this.$.errorMessageToast.hide();
  }

  protected onContinueClick_() {
    // If we are currently on data collectors selection page, send signal to
    // start data collection.
    if (this.selectedPage_ === SupportToolPageIndex.DATA_COLLECTOR_SELECTION) {
      this.browserProxy_
          .startDataCollection(
              this.$.issueDetails.getIssueDetails(),
              this.$.dataCollectors.getDataCollectors())
          .then(this.onDataCollectionStart_.bind(this));
    } else {
      this.selectedPage_ = this.selectedPage_ + 1;
    }
  }

  protected onBackClick_() {
    this.selectedPage_ = this.selectedPage_ - 1;
  }

  protected shouldHideBackButton_(): boolean {
    // Back button will only be shown on data collectors selection page.
    return this.selectedPage_ !== SupportToolPageIndex.DATA_COLLECTOR_SELECTION;
  }

  protected shouldHideContinueButtonContainer_(): boolean {
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
