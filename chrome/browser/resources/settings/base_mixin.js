// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a couple of helper methods used by several Polymer
 * elements.
 */

import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @interface */
export class BaseMixinInterface {
  /**
   * @param {string} query
   * @return {?HTMLElement}
   */
  $$(query) {}
}

/**
 * @polymer
 * @mixinFunction
 */
export const BaseMixin = dedupingMixin(superClass => {
  /**
   * @polymer
   * @mixinClass
   * @implements {BaseMixinInterface}
   */
  class BaseMixin extends superClass {
    /** @override */
    $$(query) {
      return this.shadowRoot.querySelector(query);
    }
  }

  return BaseMixin;
});
