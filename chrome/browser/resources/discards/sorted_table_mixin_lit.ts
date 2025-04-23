// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export const SortedTableMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<SortedTableMixinLitInterface> => {
  class SortedTableMixinLit extends superClass {
    static get properties() {
      return {
        /**
         * The current sort key, used for computing the appropriate sort
         * function.
         */
        sortKey: {type: String},

        /**
         * True if sorting in reverse, used for computing the appropriate
         * sort function.
         */
        sortReverse: {type: Boolean},
      };
    }

    accessor sortKey: string = '';
    accessor sortReverse: boolean = false;

    /**
     * Invoked when a header is clicked, sets a new sort key and updates
     * element styles to present the new sort key.
     */
    onSortClick(e: Event) {
      // Remove the presentation style on the old sort header.
      const oldElement = this.shadowRoot.querySelector<HTMLElement>(
          '.sort-column, .sort-column-reverse')!;
      if (oldElement) {
        oldElement.classList.remove('sort-column');
        oldElement.classList.remove('sort-column-reverse');
      }

      const target = e.currentTarget! as HTMLElement;
      const newSortKey = target.dataset['sortKey']!;
      if (newSortKey === this.sortKey) {
        this.sortReverse = !this.sortReverse;
      } else {
        this.sortKey = newSortKey;
      }

      // Update the sort key and the styles on the new sort header.
      const newClass = this.sortReverse ? 'sort-column-reverse' : 'sort-column';
      target.classList.add(newClass);
    }
  }

  return SortedTableMixinLit;
};

export interface SortedTableMixinLitInterface {
  onSortClick(e: Event): void;
  sortKey: string;
  sortReverse: boolean;
}
