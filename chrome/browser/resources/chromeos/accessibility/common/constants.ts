// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used throughout the accessibility extensions.
 */
import {TestImportManager} from './testing/test_import_manager.js';

export namespace constants {
  /**
   * Possible directions to perform tree traversals.
   * @enum {string}
   */
  export enum Dir {
    /** Search from left to right. */
    FORWARD = 'forward',

    /** Search from right to left. */
    BACKWARD = 'backward',
  }

  export interface Point {
    x: number;
    y: number;
  }

  /**
   * If a node contains more characters than this, it should not be visited
   * during object navigation.
   *
   * This number was taken from group_util.js and is an approximate average of
   * paragraph length. It's purpose is to prevent overloading tts.
   */
  export const OBJECT_MAX_CHARCOUNT = 1500;

  /** Identifier for the system voice. */
  export const SYSTEM_VOICE = 'chromeos_system_voice';

  /** Color for the ChromeVox focus ring. */
  export const FOCUS_COLOR = '#F7983A';

  /**
   * Interaction medium for the tutorial.
   * Note: keep in sync with the enum in
   * c/b/r/c/accessibility/common/tutorial/export const js.
   * TODO: Unify with the above file when ES6 is standard in ChromeVox.
   */
  export enum InteractionMedium {
    NONE = 'none',
    KEYBOARD = 'keyboard',
    TOUCH = 'touch',
    BRAILLE = 'braille',
  }
}

TestImportManager.exportForTesting(['constants', constants]);
