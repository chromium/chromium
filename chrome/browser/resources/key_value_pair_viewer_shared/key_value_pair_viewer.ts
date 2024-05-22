// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './key_value_pair_entry.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {KeyValuePairEntry, KeyValuePairEntryElement} from './key_value_pair_entry.js';
import {parseKeyValuePairEntry} from './key_value_pair_parser.js';
import {getCss} from './key_value_pair_viewer.css.js';
import {getHtml} from './key_value_pair_viewer.html.js';

// Limit file size to 10 MiB to prevent hanging on accidental upload.
const MAX_FILE_SIZE = 10485760;

export interface KeyValuePairViewerElement {
  $: {
    collapseAll: HTMLButtonElement,
    expandAll: HTMLButtonElement,
    spinner: HTMLElement,
    status: HTMLElement,
    table: HTMLElement,
    tableTitle: HTMLElement,
  };
}

export class KeyValuePairViewerElement extends CrLitElement {
  static get is() {
    return 'key-value-pair-viewer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entries: {type: String},

      loading: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  loading: boolean = false;
  entries: KeyValuePairEntry[] = [];

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    // Add event listeners for handling drag and dropping a key value pair file
    // onto the page for viewing.
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

  protected onExpandAllClick_() {
    const entries = this.shadowRoot!.querySelectorAll<KeyValuePairEntryElement>(
        'key-value-pair-entry[collapsed]');
    for (const entry of entries) {
      entry.collapsed = false;
    }
  }

  protected onCollapseAllClick_() {
    const entries = this.shadowRoot!.querySelectorAll<KeyValuePairEntryElement>(
        'key-value-pair-entry:not([collapsed])');
    for (const entry of entries) {
      entry.collapsed = true;
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
      this.importEntry_(file);
    }
  }

  /**
   * Read in an entry asynchronously and update the UI if parsing succeeds, or
   * show an error if it fails.
   */
  private importEntry_(file: File) {
    if (!file || file.size > MAX_FILE_SIZE) {
      this.showImportError_(file.name);
      return;
    }

    const reader = new FileReader();
    reader.onload = () => {
      const entry = parseKeyValuePairEntry(reader.result as string);

      if (entry === null) {
        this.showImportError_(file.name);
        return;
      }

      this.entries = entry;
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
    'key-value-pair-viewer': KeyValuePairViewerElement;
  }
}

customElements.define(KeyValuePairViewerElement.is, KeyValuePairViewerElement);
