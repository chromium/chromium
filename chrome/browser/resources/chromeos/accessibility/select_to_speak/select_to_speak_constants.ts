// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyCode} from '/common/key_code.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

export namespace SelectToSpeakConstants {
  export const SEARCH_KEY_CODE: number = KeyCode.SEARCH;
  export const CONTROL_KEY_CODE: number = KeyCode.CONTROL;
  export const READ_SELECTION_KEY_CODE: number = KeyCode.S;

  /**
   * How often (in ms) to check that the currently spoken node is
   * still valid and in the same position. Decreasing this will make
   * STS seem more reactive to page changes but decreasing it too much
   * could cause performance issues.
   */
  export const NODE_STATE_TEST_INTERVAL_MS: number = 500;

  /**
   * Max size in pixels for a region selection to be considered a paragraph
   * selection vs a selection of specific nodes. Generally paragraph
   * selection is a single click (size 0), though allow for a little
   * jitter.
   */
  export const PARAGRAPH_SELECTION_MAX_SIZE: number = 5;

  export interface VoiceSwitchingData {
    language: string|undefined;
    useVoiceSwitching: boolean;
  }
}

TestImportManager.exportForTesting(
    ['SelectToSpeakConstants', SelectToSpeakConstants]);
