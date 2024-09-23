// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for querying and modifying the global
 *     ChromeVox state, to avoid direct dependencies on the Background
 *     object and to facilitate mocking for tests.
 */
import {constants} from '/common/constants.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

// Temporarily re-define BrailleKeyEvent below, during the TypeScript migration.
import {BrailleKeyCommand} from '../common/braille/braille_key_types.js';
import {NavBraille} from '../common/braille/nav_braille.js';

type Point = constants.Point;

interface BrailleKeyEvent {
  command: BrailleKeyCommand;
  displayPosition?: number;
  brailleDots?: number;
  standardKeyCode?: string;
  standardKeyChar?: string;
  altKey?: boolean;
  ctrlKey?: boolean;
  shiftKey?: boolean;
}

export abstract class ChromeVoxState {
  static instance?: ChromeVoxState;
  static position: Record<string, Point> = {};

  protected static resolveReadyPromise_: VoidFunction;

  private static readyPromise_: Promise<void> =
      new Promise(resolve => ChromeVoxState.resolveReadyPromise_ = resolve);

  static ready(): Promise<void> {
    return ChromeVoxState.readyPromise_;
  }

  abstract get isReadingContinuously(): boolean;
  abstract set isReadingContinuously(newValue: boolean);

  abstract get talkBackEnabled(): boolean;

  /**
   * Handles a braille command.
   * @return True if evt was processed.
   */
  abstract onBrailleKeyEvent(evt: BrailleKeyEvent, content: NavBraille):
      boolean;

  abstract onIntroduceChromeVox(): void;
}

TestImportManager.exportForTesting(ChromeVoxState);
