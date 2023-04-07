// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface to prevent circular dependencies.
 */
import {TextEditHandler} from '../editing/editing.js';

import {BaseAutomationHandler} from './base_automation_handler.js';

export class DesktopAutomationInterface extends BaseAutomationHandler {
  /** @type {TextEditHandler} */
  get textEditHandler() {}

  /**
   * Sets whether document selections from actions should be ignored.
   * @param {boolean} val
   */
  ignoreDocumentSelectionFromAction(val) {}

  /** Handles native commands to move to the next or previous character. */
  onNativeNextOrPreviousCharacter() {}

  /**
   * Handles native commands to move to the next or previous word.
   * @param {boolean} isNext
   */
  onNativeNextOrPreviousWord(isNext) {}
}

/**
 * @type {DesktopAutomationInterface}
 */
DesktopAutomationInterface.instance;
