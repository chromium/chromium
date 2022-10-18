// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyCode} from '../common/key_code.js';

export class SelectToSpeakConstants {}

/** @const {number} */
SelectToSpeakConstants.SEARCH_KEY_CODE = KeyCode.SEARCH;

/** @const {number} */
SelectToSpeakConstants.CONTROL_KEY_CODE = KeyCode.CONTROL;

/** @const {number} */
SelectToSpeakConstants.READ_SELECTION_KEY_CODE = KeyCode.S;

/**
 * @typedef {{
 *   language: (string|undefined),
 *   useVoiceSwitching: boolean,
 * }}
 */
SelectToSpeakConstants.VoiceSwitchingData;

/**
 * How often (in ms) to check that the currently spoken node is
 * still valid and in the same position. Decreasing this will make
 * STS seem more reactive to page changes but decreasing it too much
 * could cause performance issues.
 * @const {number}
 */
SelectToSpeakConstants.NODE_STATE_TEST_INTERVAL_MS = 500;

/**
 * Max size in pixels for a region selection to be considered a paragraph
 * selection vs a selection of specific nodes. Generally paragraph
 * selection is a single click (size 0), though allow for a little
 * jitter.
 * @const {number}
 */
SelectToSpeakConstants.PARAGRAPH_SELECTION_MAX_SIZE = 5;
