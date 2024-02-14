// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface to prevent circular dependencies.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {TextEditHandler} from '../editing/text_edit_handler.js';

import {BaseAutomationHandler} from './base_automation_handler.js';

export abstract class DesktopAutomationInterface extends BaseAutomationHandler {
  abstract get textEditHandler(): (TextEditHandler|undefined);

  /** Sets whether document selections from actions should be ignored. */
  abstract ignoreDocumentSelectionFromAction(val: boolean): void;

  /** Handles native commands to move to the next or previous character. */
  abstract onNativeNextOrPreviousCharacter(): void;

  /** Handles native commands to move to the next or previous word. */
  abstract onNativeNextOrPreviousWord(isNext: boolean): void;
}

export namespace DesktopAutomationInterface {
  export let instance: DesktopAutomationInterface|undefined;
}

TestImportManager.exportForTesting(DesktopAutomationInterface);
