// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

const prefersDark = window.matchMedia('(prefers-color-scheme: dark)');

type Constructor<T> = new (...args: any[]) => T;

export const DarkModeMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<DarkModeMixinInterface> => {
      class DarkModeMixin extends superClass {
        static get properties() {
          return {
            /** Whether or not the OS is in dark mode. */
            inDarkMode: {type: Boolean},
          };
        }

        private boundOnChange_: (() => void)|null = null;
        accessor inDarkMode: boolean = prefersDark.matches;

        override connectedCallback() {
          super.connectedCallback();
          if (!this.boundOnChange_) {
            this.boundOnChange_ = () => this.onChange_();
          }
          prefersDark.addListener(this.boundOnChange_);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          prefersDark.removeListener(this.boundOnChange_);
          this.boundOnChange_ = null;
        }

        private onChange_() {
          this.inDarkMode = prefersDark.matches;
        }
      }

      return DarkModeMixin;
    };

export function inDarkMode(): boolean {
  return prefersDark.matches;
}

export interface DarkModeMixinInterface {
  inDarkMode: boolean;
}
