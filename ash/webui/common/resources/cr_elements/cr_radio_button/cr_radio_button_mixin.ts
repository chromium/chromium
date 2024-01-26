// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin for cr-radio-button-like elements.
 * Forked from
 * ui/webui/resources/cr_elements/cr_radio_button/cr_radio_button_mixin.ts
 */

// clang-format off
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert, assertNotReached} from '//resources/js/assert.js';

interface PaperRippleElement {
  clear(): void;
  showAndHoldDown(): void;
}

type Constructor<T> = new (...args: any[]) => T;

export const CrRadioButtonMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CrRadioButtonMixinInterface> => {
      class CrRadioButtonMixin extends superClass implements
          CrRadioButtonMixinInterface {
        static get properties() {
          return {
            checked: {
              type: Boolean,
              value: false,
              reflectToAttribute: true,
            },

            disabled: {
              type: Boolean,
              value: false,
              reflectToAttribute: true,
              notify: true,
            },

            /**
             * Whether the radio button should be focusable or not. Toggling
             * this property sets the corresponding tabindex of the button
             * itself as well as any links in the button description.
             */
            focusable: {
              type: Boolean,
              value: false,
              observer: 'onFocusableChanged_',
            },

            hideLabelText: {
              type: Boolean,
              value: false,
              reflectToAttribute: true,
            },

            label: {
              type: String,
              value: '',  // Allows hidden$= binding to run without being set.
            },

            name: {
              type: String,
              notify: true,
              reflectToAttribute: true,
            },

            /**
             * Holds the tabIndex for the radio button.
             */
            buttonTabIndex_: {
              type: Number,
              computed: 'getTabIndex_(focusable)',
            },
          };
        }

        checked: boolean;
        disabled: boolean;
        focusable: boolean;
        hideLabelText: boolean;
        label: string;
        name: string;
        private buttonTabIndex_: number;

        override connectedCallback() {
          super.connectedCallback();
          this.addEventListener('blur', this.hideRipple_.bind(this));
          if (!document.documentElement.hasAttribute('chrome-refresh-2023')) {
            this.addEventListener('focus', this.onFocus_.bind(this));
          }
          this.addEventListener('up', this.hideRipple_.bind(this));
        }

        override focus() {
          const button = this.shadowRoot!.querySelector<HTMLElement>('#button');
          assert(button);
          button.focus();
        }

        getPaperRipple(): PaperRippleElement {
          assertNotReached();
        }

        private onFocus_() {
          this.getPaperRipple().showAndHoldDown();
        }

        private hideRipple_() {
          this.getPaperRipple().clear();
        }

        private onFocusableChanged_() {
          const links = this.querySelectorAll('a');
          links.forEach((link) => {
            // Remove the tab stop on any links when the row is unchecked.
            // Since the row is not tabbable, any links within the row
            // should not be either.
            link.tabIndex = this.checked ? 0 : -1;
          });
        }

        private getAriaChecked_(): string {
          return this.checked ? 'true' : 'false';
        }

        private getAriaDisabled_(): string {
          return this.disabled ? 'true' : 'false';
        }

        private getTabIndex_(): number {
          return this.focusable ? 0 : -1;
        }

        /**
         * When shift-tab is pressed, first bring the focus to the host
         * element. This accomplishes 2 things:
         * 1) Host doesn't get focused when the browser moves the focus
         *    backward.
         * 2) focus now escaped the shadow-dom of this element, so that
         *    it'll correctly obey non-zero tabindex ordering of the
         *    containing document.
         */
        private onInputKeydown_(e: KeyboardEvent) {
          if (e.shiftKey && e.key === 'Tab') {
            this.focus();
          }
        }
      }

      return CrRadioButtonMixin;
    });

export interface CrRadioButtonMixinInterface {
  checked: boolean;
  disabled: boolean;
  focusable: boolean;
  hideLabelText: boolean;
  label: string;
  name: string;

  getPaperRipple(): PaperRippleElement;
}
