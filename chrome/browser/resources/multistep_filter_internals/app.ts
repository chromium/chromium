// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {LogEntry as LogEntryMojo} from './multistep_filter_internals.mojom-webui.js';


const MAX_LOG_ENTRIES = 1000;

export interface LogEntry {
  timestamp: Time;
  formattedTime: string;
  navigationId: bigint;
  eventType: string;
  sourceEtldPlus1: string;
  details: string;
  searchKey: string;
}

function convertMojoTimeToJs(mojoTime: Time): Date {
  const unixEpochMs = Number(mojoTime.internalValue / 1000n - 11644473600000n);
  return new Date(unixEpochMs);
}

export class MultistepFilterInternalsAppElement extends CrLitElement {
  static get is() {
    return 'multistep-filter-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      allLogs: {type: Array},
      filterText: {type: String},
    };
  }

  accessor allLogs: LogEntry[] = [];
  accessor filterText: string = '';
  private seenTimestamps_ = new Set<string>();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    const proxy = BrowserProxyImpl.getInstance();
    this.listenerIds_.push(proxy.callbackRouter.onLogEntryAdded.addListener(
        (mojoLog: LogEntryMojo) => {
          this.handleNewLog(this.convertMojoLog(mojoLog));
        }));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const proxy = BrowserProxyImpl.getInstance();
    this.listenerIds_.forEach(id => proxy.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  override firstUpdated() {
    this.initializeMojo();
  }

  private async initializeMojo() {
    const proxy = BrowserProxyImpl.getInstance();

    const response = await proxy.handler.getBufferedLogs();
    const bufferedLogs = (response.logs || []).map((mojoLog: LogEntryMojo) => {
      return this.convertMojoLog(mojoLog);
    });

    // Combine and deduplicate based on composite key (timestamp + eventType)
    const combined = bufferedLogs.concat(this.allLogs);
    const unique: LogEntry[] = [];
    const seen = new Set<string>();
    for (const log of combined) {
      const key = `${log.timestamp.internalValue}_${log.eventType}`;
      if (!seen.has(key)) {
        seen.add(key);
        unique.push(log);
      }
    }
    this.allLogs = unique.slice(-MAX_LOG_ENTRIES);

    // Re-populate seenTimestamps_ with values left after slice to keep sync!
    this.seenTimestamps_.clear();
    for (const log of this.allLogs) {
      this.seenTimestamps_.add(
          `${log.timestamp.internalValue}_${log.eventType}`);
    }
  }

  private handleNewLog(log: LogEntry) {
    const key = `${log.timestamp.internalValue}_${log.eventType}`;
    if (this.seenTimestamps_.has(key)) {
      return;
    }
    this.seenTimestamps_.add(key);
    const updatedLogs = [...this.allLogs, log];
    if (updatedLogs.length > MAX_LOG_ENTRIES) {
      const evicted = updatedLogs.shift()!;
      this.seenTimestamps_.delete(
          `${evicted.timestamp.internalValue}_${evicted.eventType}`);
    }
    this.allLogs = updatedLogs;
  }

  private convertMojoLog(mojoLog: LogEntryMojo): LogEntry {
    const eventType = mojoLog.eventType || '';
    const formattedTime = this.formatTime_(mojoLog.timestamp);
    const details = mojoLog.details || '';

    return {
      timestamp: mojoLog.timestamp,
      sourceEtldPlus1: mojoLog.sourceEtldPlus1 || '',
      navigationId: mojoLog.navigationId,
      formattedTime,
      eventType,
      details,
      // Include more fields in search index
      searchKey:
          `${formattedTime} ${eventType} ${mojoLog.sourceEtldPlus1 || ''} ${
              mojoLog.navigationId ? mojoLog.navigationId.toString() :
                                     ''} ${details}`
              .toLowerCase(),
    };
  }


  protected onFilterInput_(e: Event) {
    this.filterText = (e.target as CrInputElement).value;
  }

  protected onClearClick_() {
    this.allLogs = [];
    this.seenTimestamps_.clear();
  }

  protected getFilteredLogs_(): LogEntry[] {
    if (!this.filterText) {
      return this.allLogs;
    }
    const searchString = this.filterText.toLowerCase();

    return this.allLogs.filter(log => {
      return log.searchKey.includes(searchString);
    });
  }

  private formatTime_(timestamp: Time): string {
    const date = convertMojoTimeToJs(timestamp);
    return date.toLocaleTimeString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'multistep-filter-internals-app': MultistepFilterInternalsAppElement;
  }
}

customElements.define(
    MultistepFilterInternalsAppElement.is, MultistepFilterInternalsAppElement);
