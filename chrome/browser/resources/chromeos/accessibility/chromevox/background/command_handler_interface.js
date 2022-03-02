// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('CommandHandlerInterface');

CommandHandlerInterface = class {
  /**
   * Handles ChromeVox commands.
   * @param {string} command
   * @return {boolean} True if the command should propagate.
   */
  onCommand(command) {}

  /**
   * A helper to object navigation to skip all static text nodes who have
   * label/description for on ancestor nodes.
   * @param {cursors.Range} current
   * @param {constants.Dir} dir
   * @return {cursors.Range} The resulting range.
   */
  skipLabelOrDescriptionFor(current, dir) {}
};

/**
 * @type {CommandHandlerInterface}
 */
CommandHandlerInterface.instance;
