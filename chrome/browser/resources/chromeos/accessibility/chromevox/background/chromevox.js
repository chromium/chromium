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

export class ChromeVox {}

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
