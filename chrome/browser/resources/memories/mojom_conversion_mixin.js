// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a mixin class for conversion of Mojom types
 * to JS types as computed bindings, e.g., [[mojoTime(timeInMs)]], or in JS,
 * e.g., this.mojoTime(timeInMs).
 */

/** @interface */
class MojomConversionMixinInterface {
  /**
   * Converts |timeInMs| obtained from JS Date objects which represents the
   * number of milliseconds since the Unix epoch (1970-01-01 00:00:00 UTC), to a
   * Mojo Time which represents the number of microseconds since the Windows
   * epoch (1601-01-01 00:00:00 UTC).
   * @param {number} timeInMs
   * @returns {!Time}
   */
  mojoTime(timeInMs) {}
}

function MojomConversionMixin(superClass) {
  return class extends superClass {
    mojoTime(timeInMs) {
      const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
      const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
      // |epochDeltaInMs| equals base::Time::kTimeTToMicrosecondsOffset.
      const epochDeltaInMs = unixEpoch - windowsEpoch;
      return {internalValue: BigInt((timeInMs + epochDeltaInMs) * 1000)};
    }
  };
}

/**
 * @constructor
 * @extends PolymerElement
 * @implements {MojomConversionMixinInterface}
 */
export const MojomConversionMixinBase = MojomConversionMixin(PolymerElement);
