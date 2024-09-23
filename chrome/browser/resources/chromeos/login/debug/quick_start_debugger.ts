// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '../components/oobe_i18n_dropdown.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './quick_start_debugger.html.js';

let quickStartDebuggerAdded = false;
export function addDebugger() {
  assert(!quickStartDebuggerAdded, 'Only one instance of the QuickStart debugger should exit!');
  const quickStartDebugger = document.createElement('quick-start-debugger');
  document.body.appendChild(quickStartDebugger);
  quickStartDebuggerAdded = true;
}

// Actions to be performed on the frontend. Called by the browser.
enum FrontendActions {
  ABOUT_TO_START_ADVERTISING = 'about_to_start_advertising',
  ABOUT_TO_STOP_ADVERTISING = 'about_to_stop_advertising',
}

export interface FrontendActionData {
  action_name: FrontendActions;
}

// Actions to be performed in the browser. Called by the frontend.
enum BrowserActions {
  START_ADVERTISING_CALLBACK = 'start_advertising_callback',
  STOP_ADVERTISING_CALLBACK = 'stop_advertising_callback',
  SET_USE_PIN = 'set_use_pin',
  INITIATE_CONNECTION = 'initiate_connection',
  AUTHENTICATE_CONNECTION = 'authenticate_connection',
  VERIFY_USER = 'verify_user',
  SEND_WIFI_CREDS = 'send_wifi_creds',
  SEND_FIDO_ASSERTION = 'send_fido_assertion',
  CLOSE_CONNECTION = 'close_connection',
  REJECT_CONNECTION = 'reject_connection',
}

export interface BrowserActionData {
  action_name: BrowserActions;
  success?: boolean;
  device_id?: string;
  wifi_ssid?: string;
  wifi_password?: string;
  email?: string;
  reason?: string;
  use_pin?: boolean;
}

enum ConnectionClosedReason {
  KCOMPLETE = 'complete',
  KUSERABORTED = 'user_aborted',
  KAUTHENTICATIONFAILED = 'auth_fail',
  KCONNECTIONLOST = 'conn_lost',
  KREQUESTTIMEDOUT = 'timeout',
  KUNKNOWNERROR = 'unknown',
}

const QuickStartDebuggerBase =
    PolymerElement as {
      new (): PolymerElement,
    };

export class QuickStartDebugger extends QuickStartDebuggerBase {
  static get is() {
    return 'quick-start-debugger' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      mainWindowHidden: {
        type: Boolean,
        value: true,
      },

      startAdvertisingPending: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      stopAdvertisingPending: {
        type: Boolean,
        value: false,
      },

      usePinForAuth: {
        type: Boolean,
        value: false,
      },

      username: {
        type: String,
        value: 'testuser@gmail.com',
      },

      deviceId: {
        type: String,
        value: 'a1a2a3',
      },

      wifiSsid: {
        type: String,
        value: 'TestWiFi',
      },

      wifiPwd: {
        type: String,
        value: 'TestPwd',
      },

      connCloseReasons: {
        type: Array,
        value: Object.values(ConnectionClosedReason),
        readOnly: true,
      },
    };
  }

  private mainWindowHidden: boolean;
  private startAdvertisingPending: boolean;
  private stopAdvertisingPending: boolean;
  private usePinForAuth: boolean;
  private username: string;
  private deviceId: string;
  private wifiSsid: string;
  private wifiPwd: string;
  private connCloseReasons: ConnectionClosedReason;

  constructor() {
    super();
    // Expose the debugger as a global object.
    assert(!globalThis.oobeQuickStartDebugger);
    globalThis.oobeQuickStartDebugger =
      this.onFrontendActionReceived.bind(this);
  }

  private toggleVisibility(): void {
    this.mainWindowHidden = !this.mainWindowHidden;
  }

  private onStartAdvertisingTrueClicked(): void {
    this.startAdvertisingPending = false;
    this.sendActionToBrowser({
      action_name: BrowserActions.START_ADVERTISING_CALLBACK,
      success: true,
    });
  }

  private onStartAdvertisingFalseClicked(): void {
    this.startAdvertisingPending = false;
    this.sendActionToBrowser({
      action_name: BrowserActions.START_ADVERTISING_CALLBACK,
      success: false,
    });
  }

  private onStopAdvertisingFalseClicked(): void {
    this.stopAdvertisingPending = false;
    this.sendActionToBrowser({
      action_name: BrowserActions.STOP_ADVERTISING_CALLBACK,
    });
  }

  private onInitiateConnectionClicked(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.INITIATE_CONNECTION,
      device_id: this.deviceId,
    });
  }

  private onAuthenticateConnectionClicked(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.AUTHENTICATE_CONNECTION,
      device_id: this.deviceId,
    });
  }

  private onVerifyUserClicked(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.VERIFY_USER,
    });
  }

  private onSendWifiCredentialsClicked(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.SEND_WIFI_CREDS,
      wifi_ssid: this.wifiSsid,
      wifi_password: this.wifiPwd,
    });
  }

  private onSendFidoAssertionClicked(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.SEND_FIDO_ASSERTION,
      email: this.username,
    });
  }

  private onRejectConnectionClicked(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.REJECT_CONNECTION,
    });
  }

  private onCloseConnectionClicked(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.CLOSE_CONNECTION,
      reason: 'asd',
    });
  }

  private onUsePinChanged(): void {
    this.sendActionToBrowser({
      action_name: BrowserActions.SET_USE_PIN,
      use_pin: this.usePinForAuth,
    });
  }

  private sendActionToBrowser(data: BrowserActionData): void {
    chrome.send('quickStartDebugger.PerformAction', [data]);
  }

  private onFrontendActionReceived(data: FrontendActionData): void {
    assert(data.action_name);
    if (data.action_name === FrontendActions.ABOUT_TO_START_ADVERTISING) {
      this.startAdvertisingPending = true;
    } else if (data.action_name === FrontendActions.ABOUT_TO_STOP_ADVERTISING) {
      this.stopAdvertisingPending = true;
    }
  }

  private onCloseReasonSelected(): void { }
}

declare global {
  function oobeQuickStartDebugger(data: FrontendActionData): void;
  interface HTMLElementTagNameMap {
    [QuickStartDebugger.is]: QuickStartDebugger;
  }
}

customElements.define(QuickStartDebugger.is, QuickStartDebugger);
