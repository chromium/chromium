// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ContentRestriction, DataTransferEndpoint, DlpEvent, DlpEvent_Mode, DlpEvent_Restriction, DlpEvent_UserType, EndpointType, EventDestination, FileDatabaseEntry, Level, PageHandlerInterface, WebContentsInfo} from './dlp_internals.mojom-webui.js';
import {PageHandler, ReportingObserverReceiver} from './dlp_internals.mojom-webui.js';
import {getTemplate} from './dlp_internals_ui.html.js';
import {ContentRestrictionMap, DestinationComponentMap, EndpointTypeMap, EventModeMap, EventRestrictionMap, EventUserTypeMap, LevelMap, WebContentsElement} from './dlp_utils.js';

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
      webContentsElements_: Array,
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.fetchClipboardSourceInfo();
    this.fetchContentRestrictionsInfo();
    this.fetchFilesDatabaseEntries();
  }

  // Names of the top level page tabs.
  private tabNames_: string[] = [
    'Clipboard',
    'OnScreen',
    'Files',
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

  // Reporting events array.
  private reportingEvents_: DlpEvent[] = [];

  // Web Contents Info.
  private webContentsElements_: WebContentsElement[] = [];

  // Files Database Entries.
  private filesEntries_: FileDatabaseEntry[] = [];

  // Selected file inode number.
  private selectedFileInode_: bigint;

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
    if (source.url === null) {
      this.clipboardSourceUrl_ = 'undefined';
    } else {
      this.clipboardSourceUrl_ = source.url.url;
    }
  }

  private endpointTypeToString(type: EndpointType): string {
    return EndpointTypeMap[type] || 'invalid';
  }

  private async fetchContentRestrictionsInfo(): Promise<void> {
    this.pageHandler_.getContentRestrictionsInfo()
        .then((value: {webContentsInfo: WebContentsInfo[]}) => {
          this.setWebContentsInfo(value.webContentsInfo);
        })
        .catch((e: object) => {
          console.warn(
              `getContentRestrictionsInfo failed: ${JSON.stringify(e)}`);
        });
  }

  private setWebContentsInfo(webContentsInfo: WebContentsInfo[]) {
    this.webContentsElements_ = [];
    for (const info of webContentsInfo) {
      this.webContentsElements_.push(new WebContentsElement(info));
    }
    if (webContentsInfo.length) {
      this.notifySplices('webContentsElements_', [{
                           index: 0,
                           addedCount: this.webContentsElements_.length,
                           object: this.webContentsElements_,
                           type: 'splice',
                           removed: [],
                         }]);
    }
  }

  private async fetchFilesDatabaseEntries(): Promise<void> {
    this.pageHandler_.getFilesDatabaseEntries()
        .then((value: {dbEntries: FileDatabaseEntry[]}) => {
          this.setFilesDatabaseEntries(value.dbEntries);
        })
        .catch((e: object) => {
          console.warn(`getFilesDatabaseEntries failed: ${JSON.stringify(e)}`);
        });
  }

  private setFilesDatabaseEntries(entries: FileDatabaseEntry[]) {
    this.filesEntries_ = entries;
    if (entries.length) {
      this.notifySplices('filesEntries_', [{
                           index: 0,
                           addedCount: this.filesEntries_.length,
                           object: this.filesEntries_,
                           type: 'splice',
                           removed: [],
                         }]);
    }
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

  contentRestrictionToString(restriction: ContentRestriction): string {
    return ContentRestrictionMap[restriction];
  }

  levelToString(level: Level): string {
    return LevelMap[level];
  }

  creationTimeToString(timestampSeconds: bigint): string {
    if (timestampSeconds) {
      const timestampMilli: number = Number(timestampSeconds) * 1000;
      const timestamp: Date = new Date(timestampMilli);
      return timestamp.toLocaleString();
    }
    return 'undefined';
  }

  private onFileSelected(e: Event) {
    const selectedFile = (e.target as HTMLInputElement).value;
    this.pageHandler_.getFileInode(selectedFile.replace('C:\\fakepath\\', ''))
        .then((value: {inode: bigint|null}) => {
          if (value.inode) {
            this.selectedFileInode_ = value.inode;
          } else {
            this.selectedFileInode_ = BigInt(0);
          }
        })
        .catch((e: object) => {
          console.warn(`getFileInode failed: ${JSON.stringify(e)}`);
        });
  }
}

customElements.define(DlpInternalsUi.is, DlpInternalsUi);
