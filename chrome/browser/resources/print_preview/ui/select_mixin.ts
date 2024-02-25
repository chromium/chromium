// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Debouncer, dedupingMixin, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Helper functions for a select with timeout. Implemented by select settings
 * sections, so that the preview does not immediately begin generating and
 * freeze the dropdown when the value is changed.
 * Assumes that the elements using this mixin have no more than one select
 * element.
 */

type Constructor<T> = new (...args: any[]) => T;

export const SelectMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SelectMixinInterface> => {
      class SelectMixin extends superClass {
        static get properties() {
          return {
            selectedValue: {
              type: String,
              observer: 'onSelectedValueChange_',
            },
          };
        }

        selectedValue: string;
        private debouncer_: Debouncer|null = null;

        private onSelectedValueChange_(
            _current: string, previous: string|null|undefined) {
          // Don't trigger an extra preview request at startup.
          if (previous === undefined) {
            return;
          }

          this.debouncer_ = Debouncer.debounce(
              this.debouncer_, timeOut.after(100),
              () => this.callProcessSelectChange_());
        }

        private callProcessSelectChange_() {
          if (!this.isConnected) {
            return;
          }

          this.onProcessSelectChange(this.selectedValue);
          // For testing only
          this.dispatchEvent(new CustomEvent(
              'process-select-change', {bubbles: true, composed: true}));
        }

        onProcessSelectChange(_value: string) {}
      }

      return SelectMixin;
    });

export interface SelectMixinInterface {
  selectedValue: string;

  /**
   * Should be overridden by elements using this mixin to receive select
   * value updates.
   * @param value The new select value to process.
   */
  onProcessSelectChange(value: string): void;
}
