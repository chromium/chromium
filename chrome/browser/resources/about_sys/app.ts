// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './log_entry.js';
import './strings.m.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxyImpl, SystemLog} from './browser_proxy.js';
import {LogEntryElement} from './log_entry.js';
import {parseSystemLog} from './log_parser.js';

// Limit file size to 10 MiB to prevent hanging on accidental upload.
const MAX_FILE_SIZE = 10485760;

export interface SystemAppElement {
  $: {
    tableTitle: HTMLElement,
    status: HTMLElement,
  };
}

export class SystemAppElement extends PolymerElement {
  static get is() {
    return 'system-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      logs_: Array,

      loading_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  private logs_: SystemLog[];
  private loading_: boolean;

  private eventTracker_: EventTracker = new EventTracker();

  override async connectedCallback() {
    super.connectedCallback();

    this.loading_ = true;
    this.logs_ = await BrowserProxyImpl.getInstance().requestSystemInfo();
    this.loading_ = false;

    // Add event listeners for handling drag and dropping a system_logs.txt file
    // onto chrome://system for viewing.
    this.eventTracker_.add(
        document.documentElement, 'dragover', this.onDragOver_.bind(this),
        false);
    this.eventTracker_.add(
        document.documentElement, 'drop', this.onDrop_.bind(this), false);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  private onExpandAllClick_() {
    const logs = this.shadowRoot!.querySelectorAll<LogEntryElement>(
        'log-entry[collapsed]');
    for (const log of logs) {
      log.collapsed = false;
    }
  }

  private onCollapseAllClick_() {
    const logs = this.shadowRoot!.querySelectorAll<LogEntryElement>(
        'log-entry:not([collapsed])');
    for (const log of logs) {
      log.collapsed = true;
    }
  }

  private onDragOver_(e: DragEvent) {
    e.dataTransfer!.dropEffect = 'copy';
    e.preventDefault();
  }

  private onDrop_(e: DragEvent) {
    const file = e.dataTransfer!.files[0];
    if (file) {
      e.preventDefault();
      this.importLog_(file);
    }
  }

  /**
   * Read in a log asynchronously and update the UI if parsing succeeds, or show
   * an error if it fails.
   */
  private importLog_(file: File) {
    if (!file || file.size > MAX_FILE_SIZE) {
      this.showImportError_(file.name);
      return;
    }

    const reader = new FileReader();
    reader.onload = () => {
      const systemLog = parseSystemLog(reader.result as string);

      if (systemLog === null) {
        this.showImportError_(file.name);
        return;
      }

      this.logs_ = systemLog;
      // Reset table title and status
      this.$.tableTitle.textContent =
          loadTimeData.getStringF('logFileTableTitle', file.name);
      this.$.status.textContent = '';
    };
    reader.readAsText(file);
  }

  private showImportError_(fileName: string) {
    this.$.status.textContent = loadTimeData.getStringF('parseError', fileName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'system-app': SystemAppElement;
  }
}

customElements.define(SystemAppElement.is, SystemAppElement);
