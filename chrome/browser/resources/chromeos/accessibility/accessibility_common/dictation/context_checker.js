// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from './input_controller.js';

/** @enum {number} */
export const Context = {
  INACTIVE_INPUT_CONTROLLER: 1,
  EMPTY_EDITABLE: 2,
  NO_SELECTION: 3,
  INVALID_INPUT: 4,
  NO_PREVIOUS_MACRO: 5,
};

/** A class that can be used to specify and check contexts. */
export class ContextChecker {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @private {!Array<!Context>} */
    this.invalidContexts_ = [];
  }

  /**
   * @param {!Context} context
   * @return {!ContextChecker} |this| for chaining
   */
  add(context) {
    this.invalidContexts_.push(context);
    return this;
  }

  /** @return {?Context} */
  getFailedContext() {
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

  /**
   * @param {!Context} context
   * @return {boolean} Whether or not the context is met.
   * @private
   */
  check_(context) {
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
