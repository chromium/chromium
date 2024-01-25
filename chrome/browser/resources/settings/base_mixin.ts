// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a couple of helper methods used by several Polymer
 * elements.
 */

import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export interface BaseMixinInterface {
  $$<E extends Element = Element>(query: string): E|null;
  fire(eventName: string, detail?: any): void;
}

export const BaseMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<BaseMixinInterface> => {
      class BaseMixin extends superClass {
        $$<E extends Element = Element>(query: string) {
          return this.shadowRoot!.querySelector<E>(query);
        }

        fire(eventName: string, detail?: any) {
          this.dispatchEvent(new CustomEvent(
              eventName, {bubbles: true, composed: true, detail}));
        }
      }

      return BaseMixin;
    });
