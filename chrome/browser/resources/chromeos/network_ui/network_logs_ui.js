// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NetworkUIBrowserProxy, NetworkUIBrowserProxyImpl} from './network_ui_browser_proxy.js';

/**
 * @fileoverview
 * Polymer element for UI controlling the storing of system logs.
 */

Polymer({
  is: 'network-logs-ui',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether to store the system_logs file sent with Feedback reports.
     * @private
     */
    systemLogs_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether to filter PII in the system_logs file.
     * @private
     */
    filterPII_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether to store the zipped debugd log files.
     * @private
     */
    debugLogs_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to store the chrome logs with the zipped log files.
     * @private
     */
    chromeLogs_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to store the policies .json file.
     * @private
     */
    policies_: {
      type: Boolean,
      value: false,
    },

    /**
     * Shill debugging level.
     * @private
     */
    shillDebugging_: {
      type: String,
      value: 'unknown',
    }
  },

  observers: ['onShillDebuggingChanged_(shillDebugging_)'],

  /** @type {!NetworkUIBrowserProxy} */
  browserProxy_: NetworkUIBrowserProxyImpl.getInstance(),

  /** @override */
  attached() {},

  /* @private */
  validOptions_() {
    return this.systemLogs_ || this.policies_ || this.debugLogs_;
  },

  /* @private */
  onShillDebuggingChanged_() {
    const shillDebugging = this.shillDebugging_;
    if (!shillDebugging || shillDebugging == 'unknown')
      return;
    this.browserProxy_.setShillDebugging(shillDebugging).then((response) => {
      /*const result =*/ response.shift();
      const isError = response.shift();
      if (isError) {
        console.error('setShillDebugging: ' + shillDebugging + ' failed.');
      }
    });
  },

  /* @private */
  onStore_() {
    const options = {
      systemLogs: this.systemLogs_,
      filterPII: this.filterPII_,
      debugLogs: this.debugLogs_,
      chromeLogs: this.chromeLogs_,
      policies: this.policies_,
    };
    this.$.storeResult.innerText = this.i18n('networkLogsStatus');
    this.$.storeResult.classList.toggle('error', false);
    this.browserProxy_.storeLogs(options).then((response) => {
      const result = response.shift();
      const isError = response.shift();
      this.$.storeResult.innerText = result;
      this.$.storeResult.classList.toggle('error', isError);
    });
  },
});
