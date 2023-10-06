// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {constants} from '../../../common/constants.js';
import {CursorRange} from '../../../common/cursors/range.js';
import {Command} from '../../common/command.js';

/**
 * Interface defining the methods of performing or modifying the performance of
 * commands.
 */
export class CommandHandlerInterface {
  /**
   * Handles ChromeVox commands.
   * @param {!Command} command
   * @return {boolean} True if the command should propagate.
   */
  onCommand(command) {}

  /**
   * A helper to object navigation to skip all static text nodes who have
   * label/description for on ancestor nodes.
   * @param {CursorRange} current
   * @param {constants.Dir} dir
   * @return {CursorRange} The resulting range.
   */
  skipLabelOrDescriptionFor(current, dir) {}
}

/**
 * @type {CommandHandlerInterface}
 */
CommandHandlerInterface.instance;
