// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides braille output for ChromeVox.
 * Currently a stub; logic is being moved incrementally from Output to
 * BrailleOutput over a series of small changes.
 */
import {LogType} from '../../common/log_types.js';
import {Spannable} from '../../common/spannable.js';

import {OutputFormatLogger} from './output_logger.js';

export class BrailleOutput {
  readonly buffer: Spannable[] = [];
  readonly formatLog =
      new OutputFormatLogger('enableBrailleLogging', LogType.BRAILLE_RULE);

  equals(rhs: BrailleOutput): boolean {
    if (this.buffer.length !== rhs.buffer.length) {
      return false;
    }

    for (let i = 0; i < this.buffer.length; i++) {
      if (this.buffer[i].toString() !== rhs.buffer[i].toString()) {
        return false;
      }
    }
    return true;
  }
}
