// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin to be used by Polymer elements that define Support Tool
 * pages.
 */

import type {I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export const SupportToolPageMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SupportToolPageMixinInterface> => {
      const superClassBase = I18nMixin(superClass);

      class SupportToolPageMixin extends superClassBase implements
          SupportToolPageMixinInterface {
        $$<E extends Element = Element>(query: string) {
          return this.shadowRoot!.querySelector<E>(query);
        }

        // Puts the focus on the first header (h1) element of the page. Every
        // Support Tool page Polymer element that implements this mixin should
        // have a focusable header to describe the step.
        ensureFocusOnPageHeader() {
          this.$$<HTMLElement>('h1')!.focus();
        }
      }
      return SupportToolPageMixin;
    });

export interface SupportToolPageMixinInterface extends I18nMixinInterface {
  $$<E extends Element = Element>(query: string): E|null;

  ensureFocusOnPageHeader(): void;
}
