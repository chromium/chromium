// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from '//resources/js/custom_element.js';
import {getTrustedHTML} from '//resources/js/static_types.js';

import {getTemplate} from './diagnose_info_table.html.js';

export class DiagnoseInfoTableElement extends CustomElement {
  static get is() {
    return 'diagnose-info-table';
  }

  static override get template() {
    return getTemplate();
  }

  private tableTitle_: HTMLElement;
  private tableHead_: HTMLElement;
  private tableBody_: HTMLElement;
  private tableFooter_: HTMLElement;
  private lastTableEntries_: Array<Record<string, string>>;

  constructor() {
    super();
    this.tableTitle_ = this.getRequiredElement('caption#table-title');
    this.tableHead_ = this.getRequiredElement('thead');
    this.tableBody_ = this.getRequiredElement('tbody');
    this.tableFooter_ = this.getRequiredElement('caption#table-footer');
    this.style.display = 'none';
    this.lastTableEntries_ = [];
  }

  hideTable() {
    this.style.display = 'none';
    this.tableTitle_.textContent = '';
    this.tableHead_.innerHTML = getTrustedHTML``;
    this.tableBody_.innerHTML = getTrustedHTML``;
    this.tableFooter_.textContent = '';
  }

  visible(): boolean {
    return !(this.style.display === 'none');
  }

  updateTable(
      tableName: string, entries: Array<Record<string, string>>,
      footer: string|undefined = undefined) {
    if (entries.length === 0) {
      this.hideTable();
      return;
    }
    this.lastTableEntries_ = entries;
    this.style.display = 'block';
    this.tableTitle_.textContent = tableName;
    this.tableHead_.innerHTML = getTrustedHTML``;
    this.tableBody_.innerHTML = getTrustedHTML``;
    const tableHeadFirstRow = document.createElement('tr');
    this.tableHead_.appendChild(tableHeadFirstRow);
    for (let i: number = 0; i < entries.length; i++) {
      const entry = entries[i];
      const tableBodyRow = document.createElement('tr');
      for (const fieldName in entry) {
        if (i === 0) {
          const nameCell = document.createElement('th');
          nameCell.textContent = fieldName;
          tableHeadFirstRow.appendChild(nameCell);
        }
        const valueCell = document.createElement('td');
        valueCell.textContent = entry[fieldName]!;
        tableBodyRow.appendChild(valueCell);
      }
      this.tableBody_.appendChild(tableBodyRow);
    }
    if (footer === undefined) {
      this.tableFooter_.textContent = '';
    } else {
      this.tableFooter_.textContent = footer;
    }
  }

  outputTable(): Record<string, any> {
    const table: Record<string, any> = {};
    const name = this.tableTitle_.textContent;
    table[name!] = this.lastTableEntries_;
    return table;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnose-info-table': DiagnoseInfoTableElement;
  }
}

customElements.define(DiagnoseInfoTableElement.is, DiagnoseInfoTableElement);
