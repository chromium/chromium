// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertI18nString} from '../assert.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import * as loadTimeData from '../models/load_time_data.js';
import {ViewName} from '../type.js';

import {EnterOptions, LeaveCondition, View} from './view.js';

// This is used as an enum.
/* eslint-disable @typescript-eslint/naming-convention */
/**
 * The type of warning.
 */
export const WarningType = {
  CAMERA_PAUSED: I18nString.ERROR_MSG_CAMERA_PAUSED,
  FILESYSTEM_FAILURE: I18nString.ERROR_MSG_FILE_SYSTEM_FAILED,
  NO_CAMERA: I18nString.ERROR_MSG_NO_CAMERA,
  DISABLED_CAMERA: I18nString.ERROR_MSG_DISABLED_CAMERA,
};
/* eslint-enable @typescript-eslint/naming-convention */

/**
 * Creates the warning-view controller.
 */
export class Warning extends View {
  private errorNames: I18nString[] = [];

  constructor() {
    super(ViewName.WARNING);
  }

  /**
   * Updates the error message for the latest error-name in the stack.
   */
  private updateMessage() {
    const message = this.errorNames[this.errorNames.length - 1];
    dom.get('#error-msg', HTMLElement).textContent =
        loadTimeData.getI18nMessage(message);
  }

  override entering(nameOption?: EnterOptions): void {
    const name = assertI18nString(nameOption);

    // Remove the error-name from the stack to avoid duplication. Then make the
    // error-name the latest one to show its message.
    const index = this.errorNames.indexOf(name);
    if (index !== -1) {
      this.errorNames.splice(index, 1);
    }
    this.errorNames.push(name);
    this.updateMessage();
  }

  // If `value` is not specified in the `condition`, all the error will be
  // cleared.
  override leaving(condition: LeaveCondition): boolean {
    assert(condition.kind === 'CLOSED');

    if (condition.val === undefined) {
      this.errorNames = [];
    } else {
      // Recovered error-name for leaving the view.
      const name = assertI18nString(condition.val);

      // Remove the recovered error from the stack but don't leave the view
      // until there is no error left in the stack.
      const index = this.errorNames.indexOf(name);
      if (index !== -1) {
        this.errorNames.splice(index, 1);
      }
      if (this.errorNames.length > 0) {
        this.updateMessage();
        return false;
      }
    }
    dom.get('#error-msg', HTMLElement).textContent = '';
    return true;
  }
}
