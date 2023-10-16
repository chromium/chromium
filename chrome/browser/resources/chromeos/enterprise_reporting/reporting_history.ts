// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

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
  };
}

export class ReportingHistoryElement extends PolymerElement {
  private browserProxy: EnterpriseReportingBrowserProxy =
      EnterpriseReportingBrowserProxy.getInstance();

  static get is() {
    return 'reporting-history-element' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      loggingState: Boolean,
    };
  }

  private loggingState: boolean;

  loggingStateToString(checked: boolean) {
    return checked ? 'on' : 'off';
  }

  onToggleChange(event: CustomEvent<boolean>) {
    event.stopPropagation();

    // Deliver the value to the handler.
    this.browserProxy.handler.recordDebugState(event.detail);
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set up history table to initially show as empty.
    this.setEmptyErpTable();

    // Add a listener for the asynchronous 'setErpHistoryData' event
    // to be invoked by page handler and populate the table.
    this.browserProxy.callbackRouter.setErpHistoryData.addListener(
        (history: ErpHistoryData) => {
          this.updateErpTable(history);
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
          this.updateErpTable(historyData);
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
  private updateErpTable(history: ErpHistoryData) {
    // Reset table.
    this.$.body.replaceChildren();

    // If there are no events, present a placeholder.
    if (history.events.length === 0) {
      this.setEmptyErpTable();
      return;
    }

    // Populate the table row by the events: iterate through the history
    // in reverse order so that the most recent event shows up first.
    for (const event of history.events.reverse()) {
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
