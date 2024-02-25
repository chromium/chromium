// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_logs_ui.html.js';
import {NetworkUiBrowserProxy, NetworkUiBrowserProxyImpl} from './network_ui_browser_proxy.js';

/**
 * @fileoverview
 * Polymer element for UI controlling the storing of system logs.
 */

const NetworkLogsUiElementBase = I18nMixin(PolymerElement);

class NetworkLogsUiElement extends NetworkLogsUiElementBase {
  static get is() {
    return 'network-logs-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether to store the system_logs file sent with Feedback reports.
       */
      systemLogs_: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether to filter PII in the system_logs file.
       */
      filterPII_: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether to store the zipped debugd log files.
       */
      debugLogs_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to store the chrome logs with the zipped log files.
       */
      chromeLogs_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to store the policies .json file.
       */
      policies_: {
        type: Boolean,
        value: false,
      },

      /**
       * Shill debugging level.
       */
      shillDebugging_: {
        type: String,
        value: 'unknown',
      },
    };
  }

  static get observers() {
    return ['onShillDebuggingChanged_(shillDebugging_)'];
  }

  private systemLogs_: boolean;
  private filterPII_: boolean;
  private debugLogs_: boolean;
  private chromeLogs_: boolean;
  private policies_: boolean;
  private shillDebugging_: string;

  private browserProxy_: NetworkUiBrowserProxy =
      NetworkUiBrowserProxyImpl.getInstance();

  private validOptions_(): boolean {
    return this.systemLogs_ || this.policies_ || this.debugLogs_;
  }

  private onShillDebuggingChanged_() {
    const shillDebugging = this.shillDebugging_;
    if (!shillDebugging || shillDebugging === 'unknown') {
      return;
    }
    this.browserProxy_.setShillDebugging(shillDebugging).then(([
                                                                _,
                                                                isError,
                                                              ]) => {
      if (isError) {
        console.error('setShillDebugging: ' + shillDebugging + ' failed.');
      }
    });
  }

  private onStore_() {
    const options = {
      systemLogs: this.systemLogs_,
      filterPII: this.filterPII_,
      debugLogs: this.debugLogs_,
      chromeLogs: this.chromeLogs_,
      policies: this.policies_,
    };
    const storeResult =
        this.shadowRoot!.querySelector<HTMLElement>('#storeResult');
    assert(storeResult);
    storeResult.innerText = this.i18n('networkLogsStatus');
    storeResult.classList.toggle('error', false);
    this.browserProxy_.storeLogs(options).then(([result, isError]) => {
      storeResult.innerText = result;
      storeResult.classList.toggle('error', isError);
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkLogsUiElement.is]: NetworkLogsUiElement;
  }
}

customElements.define(NetworkLogsUiElement.is, NetworkLogsUiElement);
