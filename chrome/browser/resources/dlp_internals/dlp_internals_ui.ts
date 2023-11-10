// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DataTransferEndpoint, DlpEvent, DlpEvent_Mode, DlpEvent_Restriction, DlpEvent_UserType, EndpointType, EventDestination, PageHandler, PageHandlerInterface, ReportingObserverReceiver} from './dlp_internals.mojom-webui.js';
import {getTemplate} from './dlp_internals_ui.html.js';
import {DestinationComponentMap, EndpointTypeMap, EventModeMap, EventRestrictionMap, EventUserTypeMap} from './dlp_utils.js';

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
      reportingEvents_: Array,
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

  private reportingEvents_: DlpEvent[] = [];

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
    this.reportingEvents_.push(event);
    this.notifySplices('reportingEvents_', [{
                         index: this.reportingEvents_.length - 1,
                         addedCount: 1,
                         object: this.reportingEvents_,
                         type: 'splice',
                         removed: [],
                       }]);
  }

  destinationToString(destination: EventDestination|null|undefined): string {
    if (destination) {
      if (destination.urlPattern) {
        return destination.urlPattern;
      }
      if (destination.component) {
        return DestinationComponentMap[destination.component];
      }
    }
    return 'undefined';
  }

  restrictionToString(restriction: DlpEvent_Restriction|null|
                      undefined): string {
    if (restriction) {
      return EventRestrictionMap[restriction];
    }
    return 'undefined';
  }

  modeToString(mode: DlpEvent_Mode|null|undefined): string {
    if (mode) {
      return EventModeMap[mode];
    }
    return 'undefined';
  }

  userTypeToString(userType: DlpEvent_UserType|null|undefined): string {
    if (userType) {
      return EventUserTypeMap[userType];
    }
    return 'undefined';
  }

  timestampToString(timestampMicro: bigint): string {
    if (timestampMicro) {
      const timestampMilli: number = Number(timestampMicro) / 1000;
      const timestamp: Date = new Date(timestampMilli);
      return timestamp.toLocaleString();
    }
    return 'undefined';
  }
}

customElements.define(DlpInternalsUi.is, DlpInternalsUi);
