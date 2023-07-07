// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert_ts.js';
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

  private tableCaption_: HTMLElement;
  private tableHead_: HTMLElement;
  private tableBody_: HTMLElement;

  constructor() {
    super();
    this.tableHead_ = this.getRequiredElement<HTMLElement>('thead');
    this.tableBody_ = this.getRequiredElement<HTMLElement>('tbody');
    this.tableCaption_ = this.getRequiredElement<HTMLElement>('caption');
  }

  updateTable(tableName: string, entries: Array<Record<string, string>>) {
    assert(entries.length > 0);
    this.tableHead_.innerHTML = getTrustedHTML``;
    this.tableBody_.innerHTML = getTrustedHTML``;
    this.tableCaption_.textContent = tableName;
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
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnose-info-table': DiagnoseInfoTableElement;
  }
}

customElements.define(DiagnoseInfoTableElement.is, DiagnoseInfoTableElement);
