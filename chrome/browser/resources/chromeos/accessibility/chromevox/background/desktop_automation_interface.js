// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface to prevent circular dependencies.
 */
import {BaseAutomationHandler} from './base_automation_handler.js';
import {TextEditHandler} from './editing/editing.js';

export class DesktopAutomationInterface extends BaseAutomationHandler {
  /** @type {TextEditHandler} */
  get textEditHandler() {}

  /**
   * Sets whether document selections from actions should be ignored.
   * @param {boolean} val
   */
  ignoreDocumentSelectionFromAction(val) {}
}

/**
 * @type {DesktopAutomationInterface}
 */
DesktopAutomationInterface.instance;
