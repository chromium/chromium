// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util_ts.js';

import {BluetoothInternalsHandlerRemote, DebugLogsChangeHandlerRemote} from './bluetooth_internals.mojom-webui.js';
import {Page} from './page.js';

/** @const {string} */
const LOGS_NOT_SUPPORTED_STRING = 'Debug logs not supported';

/**
 * Page that allows user to enable/disable debug logs.
 */
export class DebugLogPage extends Page {
  /**
   * @param {!BluetoothInternalsHandlerRemote} bluetoothInternalsHandler
   */
  constructor(bluetoothInternalsHandler) {
    super('debug', 'Debug Logs', 'debug');

    /**
     * @private {?DebugLogsChangeHandlerRemote}
     */
    this.debugLogsChangeHandler_ = null;

    /** @private {?HTMLInputElement} */
    this.inputElement_ = null;

    /** @private {!HTMLDivElement} */
    this.debugContainer_ =
        /** @type {!HTMLDivElement} */ ($('debug-container'));

    bluetoothInternalsHandler.getDebugLogsChangeHandler().then((params) => {
      if (params.handler) {
        this.setUpInput(params.handler, params.initialToggleValue);
      } else {
        this.debugContainer_.textContent = LOGS_NOT_SUPPORTED_STRING;
      }
    });
  }

  /**
   * @param {!DebugLogsChangeHandlerRemote} handler
   * @param {boolean} initialInputValue
   */
  setUpInput(handler, initialInputValue) {
    this.debugLogsChangeHandler_ = handler;

    this.inputElement_ =
        /** @type {!HTMLInputElement} */ (document.createElement('input'));
    this.inputElement_.setAttribute('type', 'checkbox');
    this.inputElement_.checked = initialInputValue;
    this.inputElement_.addEventListener(
        'change', this.onToggleChange.bind(this));
    this.debugContainer_.appendChild(this.inputElement_);
  }

  onToggleChange() {
    this.debugLogsChangeHandler_.changeDebugLogsState(
        this.inputElement_.checked);
  }
}
