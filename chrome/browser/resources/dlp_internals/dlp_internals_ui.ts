// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DataTransferEndpoint, DlpEvent, EndpointType, PageHandler, PageHandlerInterface, ReportingObserverReceiver} from './dlp_internals.mojom-webui.js';
import {getTemplate} from './dlp_internals_ui.html.js';

const EndpointTypeMap = {
  [EndpointType.kDefault]: 'Default',
  [EndpointType.kUrl]: 'URL',
  [EndpointType.kClipboardHistory]: 'Clipboard History',
  [EndpointType.kUnknownVm]: 'Unknown VM',
  [EndpointType.kArc]: 'Arc',
  [EndpointType.kBorealis]: 'Borealis',
  [EndpointType.kCrostini]: 'Crostini',
  [EndpointType.kPluginVm]: 'Plugin VM',
  [EndpointType.kLacros]: 'Lacros',
};

// Polymer element DLP Internals UI.
class DlpInternalsUi extends PolymerElement {
  static get is() {
    return 'dlp-internals-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isOtr_: Boolean,
      doRulesManagerExist_: Boolean,
      showTabs_: Boolean,
      selectedTabIdx_: Number,
      tabNames_: Array,
      clipboardSourceType_: String,
      clipboardSourceUrl_: String,
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.fetchClipboardSourceInfo();
  }

  // Names of the top level page tabs.
  private tabNames_: string[] = [
    'Clipboard',
    'OnScreen',
    'Reporting',
  ];

  // Whether the profle is off the record.
  private isOtr_: boolean = false;

  // Whether DLP rules manager exists.
  private doRulesManagerExist_: boolean = false;

  // Whether the page tabs show be added.
  private showTabs_: boolean = false;

  // Index of the selected top level page tab.
  private selectedTabIdx_: number = 0;

  // Clipboard source type string.
  private clipboardSourceType_: string;

  // Clipboard source url.
  private clipboardSourceUrl_: string;

  private readonly pageHandler_: PageHandlerInterface;
  private readonly reportingObserver_: ReportingObserverReceiver;

  constructor() {
    super();

    if (loadTimeData.valueExists('isOtr')) {
      this.isOtr_ = loadTimeData.getBoolean('isOtr');
    }

    if (loadTimeData.valueExists('doRulesManagerExist')) {
      this.doRulesManagerExist_ =
          loadTimeData.getBoolean('doRulesManagerExist');
    }

    this.showTabs_ = !this.isOtr_ && this.doRulesManagerExist_;

    this.pageHandler_ = PageHandler.getRemote();
    this.reportingObserver_ = new ReportingObserverReceiver(this);
    this.pageHandler_.observeReporting(
        this.reportingObserver_.$.bindNewPipeAndPassRemote());
  }

  private async fetchClipboardSourceInfo(): Promise<void> {
    this.pageHandler_.getClipboardDataSource()
        .then((value: {source: DataTransferEndpoint|null}) => {
          this.setClipboardInfo(value.source);
        })
        .catch((e: object) => {
          console.warn(`getClipboardDataSource failed: ${JSON.stringify(e)}`);
        });
  }

  private setClipboardInfo(source: DataTransferEndpoint|null|undefined) {
    if (!source) {
      this.clipboardSourceType_ = 'undefined';
      this.clipboardSourceUrl_ = 'undefined';
      return;
    }

    this.clipboardSourceType_ = `${this.endpointTypeToString(source.type)}`;
    if (source.url === undefined) {
      this.clipboardSourceUrl_ = 'undefined';
    } else {
      this.clipboardSourceUrl_ = source.url.url;
    }
  }

  private endpointTypeToString(type: EndpointType): string {
    return EndpointTypeMap[type] || 'invalid';
  }

  /** Implements ReportingObserverInterface */
  onReportEvent(event: DlpEvent): void {
    // TODO(ayaelattar): Show it in the html page.
    console.warn(JSON.stringify(event));
  }
}

customElements.define(DlpInternalsUi.is, DlpInternalsUi);
