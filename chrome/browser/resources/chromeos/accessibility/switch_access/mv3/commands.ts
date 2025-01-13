// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ActionManager} from './action_manager.js';
import {AutoScanManager} from './auto_scan_manager.js';
import {Navigator} from './navigator.js';
import {SwitchAccess} from './switch_access.js';
import {ErrorType} from './switch_access_constants.js';

import Command = chrome.accessibilityPrivate.SwitchAccessCommand;

/** Runs user commands. */
export class SACommands {
  static instance?: SACommands;

  private commandMap_ = new Map<Command, () => void>([
    [Command.SELECT, () => ActionManager.onSelect()],
    [Command.NEXT, () => Navigator.byItem.moveForward()],
    [Command.PREVIOUS, () => Navigator.byItem.moveBackward()],
  ]);

  constructor() {
    chrome.accessibilityPrivate.onSwitchAccessCommand.addListener(
        (command: Command) => this.runCommand_(command));
  }

  static init(): void {
    if (SACommands.instance) {
      throw SwitchAccess.error(
          ErrorType.DUPLICATE_INITIALIZATION,
          'Cannot create more than one SACommands instance.');
    }
    SACommands.instance = new SACommands();
  }

  private runCommand_(command: Command): void {
    this.commandMap_.get(command)!();
    AutoScanManager.restartIfRunning();
  }
}

TestImportManager.exportForTesting(SACommands);
