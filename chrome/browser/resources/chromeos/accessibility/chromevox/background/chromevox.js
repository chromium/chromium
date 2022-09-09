// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a global object.
 */

import {constants} from '../../common/constants.js';
import {AbstractEarcons} from '../common/abstract_earcons.js';
import {BrailleInterface} from '../common/braille/braille_interface.js';
import {TtsInterface} from '../common/tts_interface.js';

export class ChromeVox {
  /**
   * Returns whether sticky mode is on, taking both the global sticky mode
   * pref and the temporary sticky mode override into account.
   *
   * @return {boolean} Whether sticky mode is on.
   */
  static isStickyModeOn() {
    if (ChromeVox.stickyOverride !== null) {
      return ChromeVox.stickyOverride;
    } else {
      return ChromeVox.isStickyPrefOn;
    }
  }
}

// Constants
/**
 * @type {TtsInterface}
 */
ChromeVox.tts;
/**
 * @type {BrailleInterface}
 */
ChromeVox.braille;
/**
 * @type {AbstractEarcons}
 */
ChromeVox.earcons = null;
/**
 * This indicates whether or not the sticky mode pref is toggled on.
 * Use ChromeVox.isStickyModeOn() to test if sticky mode is enabled
 * either through the pref or due to being temporarily toggled on.
 * @type {boolean}
 */
ChromeVox.isStickyPrefOn = false;
/**
 * If set to true or false, this value overrides ChromeVox.isStickyPrefOn
 * temporarily - in order to temporarily enable sticky mode while doing
 * 'read from here' or to temporarily disable it while using a widget.
 * @type {?boolean}
 */
ChromeVox.stickyOverride = null;
/**
 * @type {Object<string, constants.Point>}
 */
ChromeVox.position = {};
