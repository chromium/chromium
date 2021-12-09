// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertString} from '../assert.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import * as loadTimeData from '../models/load_time_data.js';
import {ViewName} from '../type.js';

import {View} from './view.js';

/**
 * The type of warning.
 * @enum {string}
 */
export const WarningType = {
  CAMERA_PAUSED: I18nString.ERROR_MSG_CAMERA_PAUSED,
  FILESYSTEM_FAILURE: I18nString.ERROR_MSG_FILE_SYSTEM_FAILED,
  NO_CAMERA: I18nString.ERROR_MSG_NO_CAMERA,
};

/**
 * @param {*} value The value to check.
 * @return {!I18nString}
 */
function assertI18nString(value) {
  assertString(value);
  assert(
      Object.values(I18nString).includes(value),
      `${value} is not a valid I18nString`);
  return /** @type {I18nString} */ (value);
}

/**
 * Creates the warning-view controller.
 */
export class Warning extends View {
  /**
   * @public
   */
  constructor() {
    super(ViewName.WARNING);

    /**
     * @type {!Array<!I18nString>}
     * @private
     */
    this.errorNames_ = [];
  }

  /**
   * Updates the error message for the latest error-name in the stack.
   * @private
   */
  updateMessage_() {
    const message = this.errorNames_[this.errorNames_.length - 1];
    dom.get('#error-msg', HTMLElement).textContent =
        loadTimeData.getI18nMessage(message);
  }

  /**
   * @override
   */
  entering(name) {
    name = assertI18nString(name);

    // Remove the error-name from the stack to avoid duplication. Then make the
    // error-name the latest one to show its message.
    const index = this.errorNames_.indexOf(name);
    if (index !== -1) {
      this.errorNames_.splice(index, 1);
    }
    this.errorNames_.push(name);
    this.updateMessage_();
  }

  /**
   * @override
   */
  leaving(...args) {
    // Recovered error-name for leaving the view.
    const name = assertI18nString(args[0]);

    // Remove the recovered error from the stack but don't leave the view until
    // there is no error left in the stack.
    const index = this.errorNames_.indexOf(name);
    if (index !== -1) {
      this.errorNames_.splice(index, 1);
    }
    if (this.errorNames_.length) {
      this.updateMessage_();
      return false;
    }
    dom.get('#error-msg', HTMLElement).textContent = '';
    return true;
  }
}
