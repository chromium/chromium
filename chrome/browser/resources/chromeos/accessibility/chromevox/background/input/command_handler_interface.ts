// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {constants} from '/common/constants.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Command} from '../../common/command.js';

/**
 * Interface defining the methods of performing or modifying the performance of
 * commands.
 */
export abstract class CommandHandlerInterface {
  /**
   * Handles ChromeVox commands.
   * @return True if the command should propagate.
   */
  abstract onCommand(command: Command): boolean;

  /**
   * A helper to object navigation to skip all static text nodes who have
   * label/description for on ancestor nodes.
   * @param {CursorRange} current
   * @param {constants.Dir} dir
   * @return {CursorRange} The resulting range.
   */
  abstract skipLabelOrDescriptionFor(current: CursorRange, dir: constants.Dir):
      CursorRange|null;
}

export namespace CommandHandlerInterface {
  export let instance: CommandHandlerInterface;
}

TestImportManager.exportForTesting(CommandHandlerInterface);
