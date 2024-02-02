// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './log_object.js';
import './shared_style.css.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './logging_tab.html.js';
import {MultideviceLogsBrowserProxy} from './multidevice_logs_browser_proxy.js';
import {LogMessage, Severity} from './types.js';

/**
 * Converts log message to string format for saved download file.
 * @param {!LogMessage} log
 * @return {string}
 */
function logToSavedString_(log) {
  // Convert to string value for |line.severity|.
  let severity;
  switch (log.severity) {
    case Severity.INFO:
      severity = 'INFO';
      break;
    case Severity.WARNING:
      severity = 'WARNING';
      break;
    case Severity.ERROR:
      severity = 'ERROR';
      break;
    case Severity.VERBOSE:
      severity = 'VERBOSE';
      break;
  }

  // Reduce the file path to just the file name for logging simplification.
  const file = log.file.substring(log.file.lastIndexOf('/') + 1);

  return `[${log.time} ${severity} ${file} (${log.line})] ${log.text}\n`;
}

Polymer({
  is: 'logging-tab',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {

    /**
     * @private {!Array<!LogMessage>}
     */
    logList_: {
      type: Array,
      value: [],
    },
  },

  /** @private {?MultideviceLogsBrowserProxy}*/
  browserProxy_: null,

  /**
   * Initialize |browserProxy_|.
   * @override
   */
  created() {
    this.browserProxy_ = MultideviceLogsBrowserProxy.getInstance();
  },

  /**
   * When the page is initialized, start listening for log message events
   * and load in the contents of its log buffer. Initialize WebUI Listeners.
   * @override
   */
  attached() {
    this.addWebUIListener(
        'multidevice-log-message-added', log => this.onLogMessageAdded_(log));
    this.addWebUIListener(
        'multidevice-log-buffer-cleared',
        () => this.onWebUILogBufferCleared_());
    this.browserProxy_.getLogMessages().then(
        logs => this.onGetLogMessages_(logs));
  },

  /**
   * Clears javascript logs displayed, but c++ log buffer remains.
   * @private
   */
  onClearLogsButtonClicked_() {
    this.clearLogBuffer_();
  },

  /**
   * Saves and downloads javascript logs that appear on the page.
   * @private
   */
  onSaveLogsButtonClicked_() {
    const blob = new Blob(
        this.getSerializedLogStrings_(), {type: 'text/plain;charset=utf-8'});
    const url = URL.createObjectURL(blob);

    const anchorElement = document.createElement('a');
    anchorElement.href = url;
    anchorElement.download =
        'multidevice_internals_logs_' + new Date().toJSON() + '.txt';
    document.body.appendChild(anchorElement);
    anchorElement.click();

    window.setTimeout(function() {
      document.body.removeChild(anchorElement);
      window.URL.revokeObjectURL(url);
    }, 0);
  },

  /**
   * Iterates through log messages in |logList_| and prepares them for download.
   * @private
   * @return {!Array<string>}
   */
  getSerializedLogStrings_() {
    // Reverse the logs so that the oldest logs appear first and the newest logs
    // appear last.
    return this.logList_.map(logToSavedString_).reverse();
  },

  /**
   * Adds a log message to the javascript log list displayed. Called from the
   * C++ WebUI handler when a log message is added to the log buffer.
   * @param {!Array<!LogMessage>} log
   * @private
   */
  onLogMessageAdded_(log) {
    this.unshift('logList_', log);
  },

  /**
   * Called in response to WebUI handler clearing log buffer.
   * @private
   */
  onWebUILogBufferCleared_() {
    this.clearLogBuffer_();
  },

  /**
   * Parses an array of log messages and adds to the javascript list sent in
   * from the initial page load.
   * @param {!Array<!LogMessage>} logs
   * @private
   */
  onGetLogMessages_(logs) {
    this.logList_ = logs.reverse().concat(this.logList_);
  },

  /**
   * Clears the javascript log buffer.
   * @private
   */
  clearLogBuffer_() {
    this.logList_ = [];
  },
});
