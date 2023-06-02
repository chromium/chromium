// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '../../components/oobe_i18n_dropdown.js';

import { assert } from '//resources/ash/common/assert.js';
import { html, PolymerElement } from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

let quickStartDebuggerAdded = false;
export function addDebugger() {
  assert(!quickStartDebuggerAdded, 'Only one instance of the QuickStart debugger should exit!');
  const quickStartDebugger = document.createElement('quick-start-debugger');
  document.body.appendChild(quickStartDebugger);
  quickStartDebuggerAdded = true;
}

// Actions to be performed on the frontend. Called by the browser.
const FrontendActions = {
  ABOUT_TO_START_ADVERTISING: 'about_to_start_advertising',
  ABOUT_TO_STOP_ADVERTISING: 'about_to_stop_advertising',
};

// Actions to be performed in the browser. Called by the frontend.
const BrowserActions = {
  START_ADVERTISING_CALLBACK: 'start_advertising_callback',
  STOP_ADVERTISING_CALLBACK: 'stop_advertising_callback',
  SET_USE_PIN: 'set_use_pin',
  INITIATE_CONNECTION: 'initiate_connection',
  AUTHENTICATE_CONNECTION: 'authenticate_connection',
  VERIFY_USER: 'verify_user',
  SEND_WIFI_CREDS: 'send_wifi_creds',
  SEND_FIDO_ASSERTION: 'send_fido_assertion',
  CLOSE_CONNECTION: 'close_connection',
  REJECT_CONNECTION: 'reject_connection',
};

const ConnectionClosedReason = {
  kComplete: 'complete',
  kUserAborted: 'user_aborted',
  kAuthenticationFailed: 'auth_fail',
  kConnectionLost: 'conn_lost',
  kRequestTimedOut: 'timeout',
  kUnknownError: 'unknown',
};

class QuickStartDebugger extends PolymerElement {
  static get is() {
    return 'quick-start-debugger';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
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

      usePinForAuth_: {
        type: Boolean,
        value: false,
      },

      username_: {
        type: String,
        value: 'testuser@gmail.com',
      },

      deviceId_: {
        type: String,
        value: 'a1a2a3',
      },

      wifi_ssid_: {
        type: String,
        value: 'TestWiFi',
      },

      wifi_pwd_: {
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

  constructor() {
    super();
    // Expose the debugger as a global object.
    assert(!globalThis.OobeQuickStartDebugger);
    globalThis.OobeQuickStartDebugger =
      this.onFrontendActionReceived.bind(this);
  }

  toggleVisibility() {
    this.mainWindowHidden = !this.mainWindowHidden;
  }

  onStartAdvertisingTrueClicked(e) {
    this.startAdvertisingPending = false;
    this.sendActionToBrowser({
      action_name: BrowserActions.START_ADVERTISING_CALLBACK,
      success: true,
    });
  }

  onStartAdvertisingFalseClicked(e) {
    this.startAdvertisingPending = false;
    this.sendActionToBrowser({
      action_name: BrowserActions.START_ADVERTISING_CALLBACK,
      success: false,
    });
  }

  onStopAdvertisingFalseClicked(e) {
    this.stopAdvertisingPending = false;
    this.sendActionToBrowser({
      action_name: BrowserActions.STOP_ADVERTISING_CALLBACK,
    });
  }

  onInitiateConnectionClicked(e) {
    this.sendActionToBrowser({
      action_name: BrowserActions.INITIATE_CONNECTION,
      device_id: this.deviceId_,
    });
  }

  onAuthenticateConnectionClicked(e) {
    this.sendActionToBrowser({
      action_name: BrowserActions.AUTHENTICATE_CONNECTION,
      device_id: this.deviceId_,
    });
  }

  onVerifyUserClicked(e) {
    this.sendActionToBrowser({
      action_name: BrowserActions.VERIFY_USER,
    });
  }

  onSendWifiCredentialsClicked(e) {
    this.sendActionToBrowser({
      action_name: BrowserActions.SEND_WIFI_CREDS,
      wifi_ssid: this.wifi_ssid_,
      wifi_password: this.wifi_pwd_,
    });
  }

  onSendFidoAssertionClicked(e) {
    this.sendActionToBrowser({
      action_name: BrowserActions.SEND_FIDO_ASSERTION,
      email: this.username_,
    });
  }

  onRejectConnectionClicked(e) {
    this.sendActionToBrowser({
      action_name: BrowserActions.REJECT_CONNECTION,
    });
  }

  onCloseConnectionClicked(e) {
    this.sendActionToBrowser({
      action_name: BrowserActions.CLOSE_CONNECTION,
      reason: 'asd',
    });
  }

  onUsePinChanged_() {
    this.sendActionToBrowser({
      action_name: BrowserActions.SET_USE_PIN,
      use_pin: this.usePinForAuth_,
    });
  }

  buttonClickAction1(e) {
    sendDictActionToBrowser();
  }

  sendActionToBrowser(data) {
    chrome.send('quickStartDebugger.PerformAction', [data]);
  }

  onFrontendActionReceived(data) {
    assert(data.action_name);
    if (data.action_name == FrontendActions.ABOUT_TO_START_ADVERTISING) {
      this.startAdvertisingPending = true;
    } else if (data.action_name == FrontendActions.ABOUT_TO_STOP_ADVERTISING) {
      this.stopAdvertisingPending = true;
    }
  }

  onCloseReasonSelected_(e) { }
}

customElements.define(QuickStartDebugger.is, QuickStartDebugger);
