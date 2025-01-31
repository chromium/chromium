// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Helper functions for an input with timeout.
 */

declare global {
  interface HTMLElementEventMap {
    'input-change': CustomEvent<string>;
  }
}

type Constructor<T> = new (...args: any[]) => T;

export const InputMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<InputMixinInterface> => {
      class InputMixin extends superClass {
        static get properties() {
          return {
            lastValue_: {
              type: String,
              value: '',
            },
          };
        }

        private lastValue_: string|null;
        /** Timeout used to delay processing of the input, in ms. */
        private timeout_: number|null = null;

        override connectedCallback() {
          super.connectedCallback();
          this.getInput().addEventListener('input', () => this.resetTimeout_());
          this.getInput().addEventListener(
              'keydown', (e: KeyboardEvent) => this.onKeyDown_(e));
        }

        getInput(): HTMLInputElement {
          assertNotReached();
        }

        /**
         * @return The delay to use for the timeout, in ms. Elements using
         *     this behavior must set this delay as data-timeout-delay on the
         *     input element returned by getInput().
         */
        private getTimeoutDelayMs_(): number {
          const delay = parseInt(this.getInput().dataset['timeoutDelay']!, 10);
          assert(!Number.isNaN(delay));
          return delay;
        }

        /**
         * Called when a key is pressed on the input.
         */
        private onKeyDown_(event: KeyboardEvent) {
          if (event.key !== 'Enter' && event.key !== 'Tab') {
            return;
          }

          this.resetAndUpdate();
        }

        /**
         * Called when a input event occurs on the textfield. Starts an input
         * timeout.
         */
        private resetTimeout_() {
          if (this.timeout_) {
            clearTimeout(this.timeout_);
          }
          this.timeout_ =
              setTimeout(() => this.onTimeout_(), this.getTimeoutDelayMs_());
        }

        /**
         * Called after a timeout after user input into the textfield.
         */
        private onTimeout_() {
          this.timeout_ = null;
          const value = this.getInput().value || '';
          if (this.lastValue_ !== value) {
            this.lastValue_ = value;
            this.dispatchEvent(new CustomEvent(
                'input-change',
                {bubbles: true, composed: true, detail: value}));
          }
        }

        resetString() {
          this.lastValue_ = null;
        }

        resetAndUpdate() {
          if (this.timeout_) {
            clearTimeout(this.timeout_);
          }
          this.onTimeout_();
        }
      }

      return InputMixin;
    });

export interface InputMixinInterface {
  /**
   * @return The cr-input or input element the behavior should use. Should be
   *     overridden by elements using this behavior.
   */
  getInput(): (CrInputElement|HTMLInputElement);

  // Resets the lastValue_ so that future inputs trigger a change event.
  resetString(): void;

  // Called to clear the timeout and update the value.
  resetAndUpdate(): void;
}
