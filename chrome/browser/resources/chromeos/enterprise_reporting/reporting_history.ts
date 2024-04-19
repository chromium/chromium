// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EnterpriseReportingBrowserProxy} from './browser_proxy.js';
import {ErpHistoryData, ErpHistoryEvent, ErpHistoryEventParameter} from './enterprise_reporting.mojom-webui.js';
import {getTemplate} from './reporting_history.html.js';

/**
 * @fileoverview Presents history of communications between Chrome and missive
 * daemon, being captured and updated while the logging is on. When logging
 * is off, the prior history is still shown, but not updated anymore.
 */

export interface ReportingHistoryElement {
  $: {
    body: HTMLDivElement,
    erpTableFilter: HTMLSelectElement,
  };
}

export class ReportingHistoryElement extends PolymerElement {
  private browserProxy: EnterpriseReportingBrowserProxy =
      EnterpriseReportingBrowserProxy.getInstance();

  // Filtering options for the table.
  private static allEvents: string = 'All events';
  private static allButUploads: string = 'All events except uploads';
  private filterOptions: string[] = [
    ReportingHistoryElement.allEvents,
    ReportingHistoryElement.allButUploads,
    'QueueAction',
    'Enqueue',
    'Flush',
    'Confirm',
    'Upload',
    'BlockedRecord',
    'BlockedDestinations',
  ];
  private selectedOption: string = ReportingHistoryElement.allEvents;
  private currentHistory: ErpHistoryData;

  static get is() {
    return 'reporting-history-element' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      loggingState: Boolean,

      filterOptions: {
        type: Array,
        value: () => [],
      },

      selectedOption: {
        type: String,
        value: '',
      },
    };
  }

  private loggingState: boolean;

  loggingStateToString(checked: boolean) {
    return checked ? 'On' : 'Off';
  }

  onToggleChange(event: CustomEvent<boolean>) {
    event.stopPropagation();

    // Deliver the value to the handler.
    this.browserProxy.handler.recordDebugState(event.detail);
  }

  onFilterChange() {
    const currentSelection: string = this.$.erpTableFilter.value;
    if (this.selectedOption !== currentSelection) {
      this.selectedOption = currentSelection;
      this.updateErpTable();
    }
  }

  onDownloadButtonClick(): void {
    // Select the table and traverse through it.
    const tableRows = this.$.body.querySelectorAll('.erp-history-table tr');
    const csv: string[] = [];
    tableRows.forEach(currentRow => {
      const row: string[] = [];
      const cols = currentRow.querySelectorAll('td, th');
      cols.forEach(currentCol => {
        let value: string = '';
        // For the erp-parameters column we need to extract the information from
        // the bullet lists, for all the other columns we just append the
        // innerHTML directly.
        if (currentCol.className === 'erp-parameters') {
          currentCol.querySelectorAll('li').forEach(el => {
            value += el.innerText + ' - ';
          });
        } else {
          value = currentCol.innerHTML;
        }
        row.push(value);
      });
      csv.push(row.join(','));
    });
    // Create the file and download it. Format: reporting_logs_DATE.csv.
    const csvFile = new Blob([csv.join('\n')], {type: 'text/csv'});
    const url = URL.createObjectURL(csvFile);
    const a = document.createElement('a');
    a.href = url;
    a.download = `reporting_logs_${new Date().toISOString()}.csv`;
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set up history table to initially show as empty.
    this.setEmptyErpTable();

    // Add a listener for the asynchronous 'setErpHistoryData' event
    // to be invoked by page handler and populate the table.
    this.browserProxy.callbackRouter.setErpHistoryData.addListener(
        (historyData: ErpHistoryData) => {
          this.currentHistory = historyData;
          this.updateErpTable();
        });
  }

  override ready() {
    super.ready();

    // Set initial history on/off state after refresh.
    this.browserProxy.handler.getDebugState().then(
        ({state}: {state: boolean}) => {
          this.loggingState = state;
        });

    // Populate history upon page refresh.
    this.browserProxy.handler.getErpHistoryData().then(
        ({historyData}: {historyData: ErpHistoryData}) => {
          this.currentHistory = historyData;
          this.updateErpTable();
        });
  }

  // Fills the table as empty (initially or upon update).
  private setEmptyErpTable() {
    const emptyRow = document.createElement('tr');
    // Pad with empty data cells, so that the alignment matches.
    emptyRow.replaceChildren(
        this.createHistoryTableDataCell('No events', 'erp-type'),
        this.composeEventParameters([], 'erp-parameters'),
        this.createHistoryTableDataCell('', 'erp-status'),
        this.createHistoryTableDataCell('', 'erp-timestamp'));
    this.$.body.appendChild(emptyRow);
  }

  // Fills the passed table element with the given history.
  private updateErpTable() {
    // Reset table.
    this.$.body.replaceChildren();

    // If there are no events, present a placeholder.
    if (this.currentHistory.events.length === 0) {
      this.setEmptyErpTable();
      return;
    }
    // If there are events we filter them by the type of event.
    const filteredEvents = this.currentHistory.events.filter(
        (event: ErpHistoryEvent) => event.call === this.selectedOption ||
            this.selectedOption === ReportingHistoryElement.allEvents ||
            (this.selectedOption === ReportingHistoryElement.allButUploads &&
             event.call !== 'Upload'));

    // If there are no events after filtering, present the placeholder.
    if (filteredEvents.length === 0) {
      this.setEmptyErpTable();
      return;
    }

    // Populate the table row by the events: iterate through the history
    // in reverse order so that the most recent event shows up first.
    // This uses the already filtered events by the user selection.
    for (const event of filteredEvents.reverse()) {
      const row = this.composeTableRow(event);
      this.$.body.appendChild(row);
    }
  }

  // Composes table row with the given history event.
  private composeTableRow(event: ErpHistoryEvent) {
    const row = document.createElement('tr');
    row.replaceChildren(
        this.createHistoryTableDataCell(
            this.erpHistoryTypeToString(event.call), 'erp-type'),
        this.composeEventParameters(event.parameters, 'erp-parameters'),
        this.createHistoryTableDataCell(event.status, 'erp-status'),
        this.createHistoryTableDataCell(
            this.timestampToString(Number(event.time)), 'erp-timestamp'));
    return row;
  }

  // Composes parameters as a list.
  private composeEventParameters(
      parameters: ErpHistoryEventParameter[], className: string) {
    const list = document.createElement('ul');
    for (const parameter of parameters) {
      const line = document.createElement('li');
      line.textContent = parameter.name + ': ' + parameter.value;
      list.appendChild(line);
    }
    const element = document.createElement('td');
    element.appendChild(list);
    element.classList.add(className);
    return element;
  }

  // Composes table data cell
  private createHistoryTableDataCell(textContent: string, className: string) {
    const td = document.createElement('td');
    td.classList.add(className);
    td.textContent = textContent;
    return td;
  }

  // Helper function to convert undefined ERP history types to 'Unknown' string.
  private erpHistoryTypeToString(erpHistoryType: string|undefined): string {
    return erpHistoryType || 'Unknown';
  }

  // Converts a given Unix timestamp into a human-readable string.
  private timestampToString(timestampSeconds: number): string {
    if (timestampSeconds === 0) {
      // This case should not normally happen.
      return 'N/A';
    }

    assert(!Number.isNaN(timestampSeconds));

    // Multiply by 1000 since the constructor expects milliseconds, but the
    // timestamps are in seconds.
    const timestamp: Date = new Date(timestampSeconds * 1000);

    // For today's timestamp, show time only.
    const now: Date = new Date();
    if (timestamp.getDate() === now.getDate()) {
      return timestamp.toLocaleTimeString();
    }

    // Otherwise show whole timestamp.
    return timestamp.toLocaleString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ReportingHistoryElement.is]: ReportingHistoryElement;
  }
}

customElements.define(ReportingHistoryElement.is, ReportingHistoryElement);
