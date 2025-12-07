// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {BluetoothBtsnoopRemote, BluetoothInternalsHandlerRemote, DebugLogsChangeHandlerRemote} from './bluetooth_internals.mojom-webui.js';
import {Page} from './page.js';

const LOGS_NOT_SUPPORTED_STRING = 'Debug logs not supported';

/**
 * Page that allows user to enable/disable debug logs.
 */
export class DebugLogPage extends Page {
  private debugLogsChangeHandler_: DebugLogsChangeHandlerRemote|null = null;
  private inputElement_: HTMLInputElement|null = null;
  private debugContainer_: HTMLDivElement;
  private bluetoothInternalsHandler_: BluetoothInternalsHandlerRemote;
  private btsnoopInterface_: BluetoothBtsnoopRemote|null = null;

  constructor(bluetoothInternalsHandler: BluetoothInternalsHandlerRemote) {
    super('debug', 'Debug Logs', 'debug');

    this.debugContainer_ =
        getRequiredElement<HTMLDivElement>('debug-container');

    this.bluetoothInternalsHandler_ = bluetoothInternalsHandler;
    this.btsnoopInterface_ = null;

    // <if expr="is_chromeos">
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

  setUpInput(
      handler: DebugLogsChangeHandlerRemote, initialInputValue: boolean) {
    this.debugLogsChangeHandler_ = handler;

    this.inputElement_ = document.createElement('input');
    this.inputElement_.setAttribute('type', 'checkbox');
    this.inputElement_.checked = initialInputValue;
    this.inputElement_.addEventListener(
        'change', this.onToggleChange.bind(this));
    this.debugContainer_.appendChild(this.inputElement_);
  }

  setUpBtmonButton() {
    const elem = getRequiredElement<HTMLInputElement>('btmon-start-btn');
    elem.addEventListener('click', this.onStartBtsnoopClick.bind(this));
    this.setBtmonButtonText('Start logging');
  }

  setBtmonButtonText(text: string) {
    const elem = getRequiredElement<HTMLInputElement>('btmon-start-btn');
    elem.textContent = text;
  }

  setBtmonStatusText(text: string) {
    const elem = getRequiredElement<HTMLDivElement>('btmon-status-bar');
    elem.textContent = text;
  }

  onToggleChange() {
    this.debugLogsChangeHandler_!.changeDebugLogsState(
        this.inputElement_!.checked);
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

    this.pageDiv.appendChild(document.importNode(
        getRequiredElement<HTMLTemplateElement>('btsnoop-template').content,
        /*deep=*/ true));
    this.setUpBtmonButton();
  }
}
