// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'StrictQueryMixin' is a mixin that adds the function strictQuery, which is
 * used to query the Shadow DOM for elements that are known to exist.
 */

import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {strictQuery} from './strict_query.js';

type Constructor<T> = new (...args: any[]) => T;

export const StrictQueryMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<StrictQueryMixinInterface> => {
      class StrictQueryMixin extends superClass implements
          StrictQueryMixinInterface {
        /**
         * Queries |selector| on the element's shadowRoot and returns the first
         * matching element. Throws an exception if there is no resulting
         * element or if element is not of type |type|.
         */
        strictQuery<T>(selector: string, type: Constructor<T>): T {
          return strictQuery(selector, this.shadowRoot, type);
        }

        /**
         * Convenience wrapper around `strictQuery` that queries for
         * div elements.
         */
        strictQueryDiv(selector: string): HTMLDivElement {
          return this.strictQuery(selector, HTMLDivElement);
        }

        /**
         * Convenience wrapper around `strictQuery` that queries for
         * span elements.
         */
        strictQuerySpan(selector: string): HTMLSpanElement {
          return this.strictQuery(selector, HTMLSpanElement);
        }
      }

      return StrictQueryMixin;
    });

export interface StrictQueryMixinInterface {
  strictQuery<T>(selector: string, type: Constructor<T>): T;
  strictQueryDiv(selector: string): HTMLDivElement;
  strictQuerySpan(selector: string): HTMLSpanElement;
}
