// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '../testing/test_import_manager.js';

import {InputController} from './input_controller.js';

export enum Context {
  INACTIVE_INPUT_CONTROLLER = 1,
  EMPTY_EDITABLE = 2,
  NO_SELECTION = 3,
  INVALID_INPUT = 4,
  NO_PREVIOUS_MACRO = 5,
}

/** A class that can be used to specify and check contexts. */
export class ContextChecker {
  private inputController_: InputController;
  private invalidContexts_: Context[] = [];
  constructor(inputController: InputController) {
    this.inputController_ = inputController;
  }

  /** @return |this| for chaining */
  add(context: Context): ContextChecker {
    this.invalidContexts_.push(context);
    return this;
  }

  getFailedContext(): Context|null {
    if (!this.inputController_.isActive()) {
      return Context.INACTIVE_INPUT_CONTROLLER;
    }

    for (const context of this.invalidContexts_) {
      if (this.check_(context)) {
        return context;
      }
    }

    return null;
  }

  private check_(context: Context): boolean {
    const data = this.inputController_.getEditableNodeData();
    if (!data) {
      return false;
    }

    switch (context) {
      case Context.EMPTY_EDITABLE:
        return Boolean(!data.value);
      case Context.NO_SELECTION:
        return data.selStart === data.selEnd;
    }

    return false;
  }
}

TestImportManager.exportForTesting(['Context', Context]);
