// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes that handle the ChromeVox range.
 */
import {CursorRange} from '../../common/cursors/range.js';

import {ChromeVoxState} from './chromevox_state.js';

/**
 * An interface implemented by objects to observe ChromeVox range changes.
 * @interface
 */
export class ChromeVoxRangeObserver {
  /**
   * @param {CursorRange} range The new range.
   * @param {boolean=} opt_fromEditing
   */
  onCurrentRangeChanged(range, opt_fromEditing = undefined) {}
}

/**
 * A class that handles tracking and changes to the ChromeVox range.
 *
 * ================ THIS CLASS IS MID-MIGRATION ================
 *
 * The logic relating to the ChromeVox range is being moved here from
 * ChromeVoxState in small chunks. During this transition, the logic will be
 * split between those two locations.
 */
export class ChromeVoxRange {
  /** @param {ChromeVoxRangeObserver} observer */
  static addObserver(observer) {
    ChromeVoxRange.observers_.push(observer);
  }

  /** @param {ChromeVoxRangeObserver} observer */
  static removeObserver(observer) {
    const index = ChromeVoxRange.observers_.indexOf(observer);
    if (index > -1) {
      ChromeVoxRange.observers_.splice(index, 1);
    }
  }

  /** @public {CursorRange} */
  static get current() {
    // TODO(anastasi): Move ownership of currentRange to ChromeVoxRange.
    return ChromeVoxState.instance.currentRange;
  }

  /**
   * @param {CursorRange} range The new range.
   * @param {boolean=} opt_fromEditing
   */
  static onCurrentRangeChanged(range, opt_fromEditing = undefined) {
    for (const observer of ChromeVoxRange.observers_) {
      observer.onCurrentRangeChanged(range, opt_fromEditing);
    }
  }
}

/** @private {!Array<ChromeVoxRangeObserver>} */
ChromeVoxRange.observers_ = [];
