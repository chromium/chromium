// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;
export type AbstractConstructor<T> = abstract new (...args: any[]) => T;

/**
 * Helper functions for custom elements that implement a scan setting using a
 * single select element. Elements that use this behavior are required to
 * implement getOptionAtIndex(), sortOptions(), and isDefaultOption().
 */
export const SelectMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>, U>(superClass: T): Function&
    {prototype: SelectMixinInterface<U>} => {
      abstract class SelectMixin extends superClass implements
          SelectMixinInterface<U> {
        static get properties() {
          return {
            disabled: Boolean,

            options: {
              type: Array,
              value: () => [],
            },

            selectedOption: {
              type: String,
              notify: true,
            },
          };
        }

        static get observers() {
          return ['optionsChanged(options.*)'];
        }

        disabled: boolean;
        options: U[];
        selectedOption: string;

        abstract getOptionAtIndex(index: number): string;
        abstract isDefaultOption(option: U): boolean;
        abstract sortOptions(): void;

        /**
         * Get the index of the default option if it exists. If not, use the
         * index of the first option in the options array.
         */
        private getDefaultSelectedIndex(): number {
          assert(this.options.length > 0);

          const defaultIndex = this.options.findIndex((option: U) => {
            return this.isDefaultOption(option);
          });

          return defaultIndex === -1 ? 0 : defaultIndex;
        }

        private getSelectElement(): HTMLSelectElement {
          return this.shadowRoot!.querySelector('select')!;
        }

        /**
         * Sorts the options and sets the selected option when options change.
         */
        private optionsChanged() {
          if (this.options.length > 1) {
            this.sortOptions();
          }

          if (this.options.length > 0) {
            const selectedOptionIndex = this.getDefaultSelectedIndex();
            this.selectedOption = this.getOptionAtIndex(selectedOptionIndex);
            this.getSelectElement().selectedIndex = selectedOptionIndex;
          }
        }
      }

      return SelectMixin;
    });

export interface SelectMixinInterface<T> {
  disabled: boolean;
  options: T[];
  selectedOption: string;
  getOptionAtIndex(index: number): string;
  isDefaultOption(option: T): boolean;
  sortOptions(): void;
}
