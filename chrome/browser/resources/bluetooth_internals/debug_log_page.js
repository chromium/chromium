// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DebugLogPage, served from chrome://bluetooth-internals/.
 */
cr.define('debug_log_page', function() {
  /** @const {string} */
  const LOGS_NOT_SUPPORTED_STRING = 'Debug logs not supported';

  /**
   * Page that allows user to enable/disable debug logs.
   */
  class DebugLogPage extends cr.ui.pageManager.Page {
    /**
     * @param {!mojom.BluetoothInternalsHandlerRemote} bluetoothInternalsHandler
     */
    constructor(bluetoothInternalsHandler) {
      super('debug', 'Debug Logs', 'debug');

      /**
       * @private {?mojom.DebugLogsChangeHandlerRemote}
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
     * @param {!mojom.DebugLogsChangeHandlerRemote} handler
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

  return {
    DebugLogPage: DebugLogPage,
  };
});
