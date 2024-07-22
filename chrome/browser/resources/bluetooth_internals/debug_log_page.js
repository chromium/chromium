// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

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

    this.bluetoothInternalsHandler_ = bluetoothInternalsHandler;
    this.btsnoopInterface_ = null;

    // <if expr="chromeos_ash">
    this.prepareBtsnoopTemplate();
    // </if>

    this.bluetoothInternalsHandler_.getDebugLogsChangeHandler().then(
        (params) => {
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

  setUpBtmonButton() {
    const elem = /** @type {!HTMLInputElement} */ ($('btmon-start-btn'));
    elem.addEventListener('click', this.onStartBtsnoopClick.bind(this));
    this.setBtmonButtonText('Start logging');
  }

  setBtmonButtonText(text) {
    const elem = /** @type {!HTMLInputElement} */ ($('btmon-start-btn'));
    elem.textContent = text;
  }

  setBtmonStatusText(text) {
    const elem = /** @type {!HTMLDivElement} */ ($('btmon-status-bar'));
    elem.textContent = text;
  }

  onToggleChange() {
    this.debugLogsChangeHandler_.changeDebugLogsState(
        this.inputElement_.checked);
  }

  onStartBtsnoopClick() {
    this.btsnoopInterface_ ? this.onStopBtsnoop() : this.onStartBtsnoop();
  }

  async onStartBtsnoop() {
    const {btsnoop: btsnoopInterface} =
        await this.bluetoothInternalsHandler_.startBtsnoop();
    if (btsnoopInterface != null) {
      this.setBtmonStatusText('Logging is ongoing.');
      this.setBtmonButtonText('Stop logging');
      this.btsnoopInterface_ = btsnoopInterface;
    } else {
      this.setBtmonStatusText('Fail to start logging.');
      this.btsnoopInterface_ = null;
    }
  }

  async onStopBtsnoop() {
    if (!this.btsnoopInterface_) {
      return;
    }

    const {success} = await this.btsnoopInterface_.stop();
    if (success) {
      this.setBtmonStatusText(
          'Logging is stopped. Log is saved to Downloads as capture.btsnoop.');
    } else {
      this.setBtmonStatusText('Fail to save debug log.');
    }
    this.setBtmonButtonText('Start logging');
    this.btsnoopInterface_ = null;
  }

  async prepareBtsnoopTemplate() {
    const {enabled} =
        await this.bluetoothInternalsHandler_.isBtsnoopFeatureEnabled();
    if (!enabled) {
      return;
    }

    this.pageDiv.appendChild(
        document.importNode($('btsnoop-template').content, true /* deep */));
    this.setUpBtmonButton();
  }
}
