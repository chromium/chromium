// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface to prevent circular dependencies.
 */

goog.provide('DesktopAutomationInterface');

goog.require('BaseAutomationHandler');
goog.require('editing.TextEditHandler');

DesktopAutomationInterface = class extends BaseAutomationHandler {
  /** @type {editing.TextEditHandler} */
  get textEditHandler() {}

  /**
   * Sets whether document selections from actions should be ignored.
   * @param {boolean} val
   */
  ignoreDocumentSelectionFromAction(val) {}
};

/**
 * @type {DesktopAutomationInterface}
 */
DesktopAutomationInterface.instance;
