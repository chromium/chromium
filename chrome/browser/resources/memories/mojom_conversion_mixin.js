// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a mixin class for conversion of Mojom types
 * to JS types as computed bindings, e.g., [[decodeMojoString16(mojoString16)]],
 * or in JS, e.g., this.decodeMojoString16(mojoString16).
 */

/** @interface */
class MojomConversionMixinInterface {
  /**
   * Converts a Mojo String16 to a JS string.
   * @param {?String16} str
   * @return {string}
   */
  decodeMojoString16(str) {}
}

function MojomConversionMixin(superClass) {
  return class extends superClass {
    decodeMojoString16(str) {
      return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
    }
  };
}

/**
 * @constructor
 * @extends PolymerElement
 * @implements {MojomConversionMixinInterface}
 */
export const MojomConversionMixinBase = MojomConversionMixin(PolymerElement);
