// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'OobeBaseMixin' is a mixin for oobe-components to provide generic shared
 * functionality.
 */

type Constructor<T> = new (...args: any[]) => T;

export const OobeBaseMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<OobeBaseMixinInterface> => {
      class OobeBaseMixinInternal extends superClass implements
          OobeBaseMixinInterface {
        onBeforeShow(): void {}
        onBeforeHide(): void {}

        /**
         * Returns element that will receive focus.
         */
        get defaultControl(): HTMLElement|null {
          return this;
        }
      }

      return OobeBaseMixinInternal;
    });

export interface OobeBaseMixinInterface {
  onBeforeShow(...data: any[]): void;
  onBeforeHide(): void;
  get defaultControl(): HTMLElement|null;
}
