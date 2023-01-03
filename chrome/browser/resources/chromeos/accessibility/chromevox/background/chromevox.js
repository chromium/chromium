// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a global object that holds references to the three
 * different output engines.
 */
import {AbstractEarcons} from '../common/abstract_earcons.js';
import {TtsInterface} from '../common/tts_interface.js';

import {BrailleInterface} from './braille/braille_interface.js';

export const ChromeVox = {
  /** @type {BrailleInterface} */
  braille: null,
  /** @type {AbstractEarcons} */
  earcons: null,
  /** @type {TtsInterface} */
  tts: null,
};
