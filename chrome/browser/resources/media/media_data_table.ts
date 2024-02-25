// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

/**
 * TODO(beccahughes): Description
 */
export class MediaDataTable {
  private table_: HTMLElement;
  private data_: Array<{[key: string]: any}> = [];
  private delegate_: MediaDataTableDelegate;

  constructor(table: HTMLElement, delegate: MediaDataTableDelegate) {
    this.table_ = table;
    this.delegate_ = delegate;

    // Set table header sort handlers.
    const headers = this.table_.querySelectorAll('th[sort-key]');
    headers.forEach(header => {
      header.addEventListener('click', this.handleSortClick_.bind(this));
    });
  }

  private handleSortClick_(e: Event) {
    const target = e.target as HTMLElement;
    assert(target);
    if (target.classList.contains('sort-column')) {
      target.toggleAttribute('sort-reverse');
    } else {
      const sortColumn = document.querySelector<HTMLElement>('.sort-column');
      assert(sortColumn);
      sortColumn.classList.remove('sort-column');
      target.classList.add('sort-column');
    }

    this.render();
  }

  render() {
    // Find the body of the table and clear it.
    const body = this.table_.querySelectorAll('tbody')[0]!;
    (body.innerHTML as string | TrustedHTML) =
        window.trustedTypes ? window.trustedTypes.emptyHTML : '';

    // Get the sort key from the columns to determine which data should be in
    // which column.
    const headerCells = Array.from(this.table_.querySelectorAll('thead th'));
    const dataAndSortKeys = headerCells.map((e) => {
      return e.getAttribute('sort-key') ? e.getAttribute('sort-key') :
                                          e.getAttribute('data-key');
    });

    const currentSortCol = this.table_.querySelectorAll('.sort-column')[0]!;
    const currentSortKey = currentSortCol.getAttribute('sort-key') || '';
    const currentSortReverse = currentSortCol.hasAttribute('sort-reverse');

    // Sort the data based on the current sort key.
    this.data_.sort((a, b) => {
      return (currentSortReverse ? -1 : 1) *
          this.delegate_.compareTableItem(currentSortKey, a, b);
    });

    // Generate the table rows.
    this.data_.forEach((dataRow) => {
      const tr = document.createElement('tr');
      body.appendChild(tr);

      dataAndSortKeys.forEach((key) => {
        const td = document.createElement('td');

        // Keys with a period denote nested objects.
        let data = dataRow;
        const expandedKey = key!.split('.');
        expandedKey.forEach((k) => {
          data = data[k];
          key = k;
        });

        this.delegate_.insertDataField(td, data, key!, dataRow);
        tr.appendChild(td);
      });
    });
  }

  /**
   * @param data The data to update
   */
  setData(data: object[]) {
    this.data_ = data;
    this.render();
  }
}

export interface MediaDataTableDelegate {
  /**
   * Formats a field to be displayed in the data table and inserts it into the
   * element.
   * @param dataRow This is the row itself in case we need extra
   *   data to render the field.
   */
  insertDataField(td: Element, data: object, key: string, dataRow: object):
      void;

  /**
   * Compares two objects based on |sortKey|.
   * @param sortKey The name of the property to sort by.
   * @param a The first object to compare.
   * @param b The second object to compare.
   * @return A negative number if |a| should be ordered
   *     before |b|, a positive number otherwise.
   */
  compareTableItem(sortKey: string, a: object, b: object): number;
}
