// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This behavior bundles functionality common to sortable tables.
 *
 * @polymerBehavior
 */
export const SortedTableBehavior = {
  properties: {
    /**
     * The current sort key, used for computing the appropriate sort function.
     */
    sortKey: {
      type: String,
    },

    /**
     * True if sorting in reverse, used for computing the appropriate sort
     *     function.
     */
    sortReverse: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Sets a new sort key for this item.
   * @param {string} sortKey The new sort key.
   * @public
   */
  setSortKey: function(sortKey) {
    this.sortKey = sortKey;
  },

  /**
   * Invoked when a header is clicked, sets a new sort key and updates
   * element styles to present the new sort key.
   * @param {Event} e The event.
   * @public
   */
  onSortClick: function(e) {
    // Remove the presentation style on the old sort header.
    const oldElement = this.$$('.sort-column, .sort-column-reverse');
    if (oldElement) {
      oldElement.classList.remove('sort-column');
      oldElement.classList.remove('sort-column-reverse');
    }

    const newSortKey = e.currentTarget.dataset.sortKey;
    if (newSortKey == this.sortKey) {
      this.sortReverse = !this.sortReverse;
    } else {
      this.setSortKey(newSortKey);
    }

    // Update the sort key and the styles on the new sort header.
    const newClass = this.sortReverse ? 'sort-column-reverse' : 'sort-column';
    e.currentTarget.classList.add(newClass);
  },
};
