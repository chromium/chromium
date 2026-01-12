// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin to be used by Lit elements that define Support Tool
 * pages.
 */

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import type {I18nMixinLitInterface} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export const SupportToolPageMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<SupportToolPageMixinLitInterface> => {
      const superClassBase = I18nMixinLit(superClass);

      class SupportToolPageMixinLit extends superClassBase implements
          SupportToolPageMixinLitInterface {
        $$<E extends Element = Element>(query: string) {
          return this.shadowRoot.querySelector<E>(query);
        }

        // Puts the focus on the first header (h1) element of the page. Every
        // Support Tool page Lit element that implements this mixin should
        // have a focusable header to describe the step.
        ensureFocusOnPageHeader() {
          this.$$<HTMLElement>('h1')!.focus();
        }
      }
      return SupportToolPageMixinLit;
    };

export interface SupportToolPageMixinLitInterface extends
    I18nMixinLitInterface {
  $$<E extends Element = Element>(query: string): E|null;
  ensureFocusOnPageHeader(): void;
}
