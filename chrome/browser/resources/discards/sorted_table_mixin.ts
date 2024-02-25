// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export const SortedTableMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SortedTableMixinInterface> => {
      class SortedTableMixin extends superClass {
        static get properties() {
          return {
            /**
             * The current sort key, used for computing the appropriate sort
             * function.
             */
            sortKey: String,

            /**
             * True if sorting in reverse, used for computing the appropriate
             * sort function.
             */
            sortReverse: Boolean,
          };
        }

        sortKey: string;
        sortReverse: boolean;

        /** Sets a new sort key for this item. */
        setSortKey(sortKey: string) {
          this.sortKey = sortKey;
        }

        /**
         * Invoked when a header is clicked, sets a new sort key and updates
         * element styles to present the new sort key.
         */
        onSortClick(e: Event) {
          // Remove the presentation style on the old sort header.
          const oldElement = this.shadowRoot!.querySelector<HTMLElement>(
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
            this.setSortKey(newSortKey);
          }

          // Update the sort key and the styles on the new sort header.
          const newClass =
              this.sortReverse ? 'sort-column-reverse' : 'sort-column';
          target.classList.add(newClass);
        }
      }

      return SortedTableMixin;
    });

interface SortedTableMixinInterface {
  setSortKey(sortKey: string): void;
  onSortClick(e: Event): void;
}
