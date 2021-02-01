// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  /**
   * TODO(beccahughes): Description
   */
export class MediaDataTable {
  /**
   * @param {HTMLElement} table
   * @param {!MediaDataTableDelegate} delegate
   */
  constructor(table, delegate) {
    /** @private {HTMLElement} */
    this.table_ = table;

    /** @private {Array<Object>} */
    this.data_ = [];

    /** @private {MediaDataTableDelegate} */
    this.delegate_ = delegate;

    // Set table header sort handlers.
    const headers = this.table_.querySelectorAll('th[sort-key]');
    headers.forEach(header => {
      header.addEventListener('click', this.handleSortClick_.bind(this));
    });
  }

  handleSortClick_(e) {
    if (e.target.classList.contains('sort-column')) {
      e.target.toggleAttribute('sort-reverse');
    } else {
      document.querySelector('.sort-column').classList.remove('sort-column');
      e.target.classList.add('sort-column');
    }

    this.render();
  }

  render() {
    // Find the body of the table and clear it.
    const body = this.table_.querySelectorAll('tbody')[0];
    body.innerHTML = trustedTypes.emptyHTML;

    // Get the sort key from the columns to determine which data should be in
    // which column.
    const headerCells = Array.from(this.table_.querySelectorAll('thead th'));
    const dataAndSortKeys = headerCells.map((e) => {
      return e.getAttribute('sort-key') ? e.getAttribute('sort-key') :
                                          e.getAttribute('data-key');
    });

    const currentSortCol = this.table_.querySelectorAll('.sort-column')[0];
    const currentSortKey = currentSortCol.getAttribute('sort-key');
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
        const expandedKey = key.split('.');
        expandedKey.forEach((k) => {
          data = data[k];
          key = k;
        });

        this.delegate_.insertDataField(td, data, key, dataRow);
        tr.appendChild(td);
      });
    });
  }

  /**
   * @param {Array} data The data to update
   */
  setData(data) {
    this.data_ = data;
    this.render();
  }
}

/** @interface */
export class MediaDataTableDelegate {
  /**
   * Formats a field to be displayed in the data table and inserts it into the
   * element.
   * @param {Element} td
   * @param {?Object} data
   * @param {string} key
   * @param {Object} dataRow This is the row itself in case we need extra
   *   data to render the field.
   */
  insertDataField(td, data, key, dataRow) {}

  /**
   * Compares two objects based on |sortKey|.
   * @param {string} sortKey The name of the property to sort by.
   * @param {?Object} a The first object to compare.
   * @param {?Object} b The second object to compare.
   * @return {number} A negative number if |a| should be ordered
   *     before |b|, a positive number otherwise.
   */
  compareTableItem(sortKey, a, b) {}
}
