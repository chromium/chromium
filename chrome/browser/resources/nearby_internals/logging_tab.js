// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './log_object.js';
import './shared_style.css.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyLogsBrowserProxy} from './cross_device_logs_browser_proxy.js';
import {getTemplate} from './logging_tab.html.js';
import {LogMessage, LogProvider, SelectOption, Severity} from './types.js';

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

/** @type {LogProvider} */
const nearbyShareLogProvider = {
  messageAddedEventName: 'log-message-added',
  bufferClearedEventName: 'log-buffer-cleared',
  logFilePrefix: 'nearby_internals_logs_',
  getLogMessages: () => NearbyLogsBrowserProxy.getInstance().getLogMessages(),
};

/** @type {LogProvider} */
const quickPairLogProvider = {
  messageAddedEventName: 'quick-pair-log-message-added',
  bufferClearedEventName: 'quick-pair-log-buffer-cleared',
  logFilePrefix: 'fast_pair_logs_',
  getLogMessages: () =>
      NearbyLogsBrowserProxy.getInstance().getQuickPairLogMessages(),
};

/**
 * Gets a log provider instance for a feature.
 * @param {!string} feature
 * @return {?LogProvider}
 */
function getLogProvider(feature) {
  switch (feature) {
    case 'nearby-share':
      return nearbyShareLogProvider;
    case 'quick-pair':
      return quickPairLogProvider;
    default:
      return null;
  }
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

    /**
     * @private {!Array<!LogMessage>}
     */
    filteredLogList_: {
      type: Array,
      value: [],
    },

    /** @private {!string} */
    feature: {
      type: String,
    },

    /** @private {!string} */
    currentFilter: {
      type: String,
    },

    /** @private {!Severity} */
    currentSeverity: {
      type: Severity,
      value: Severity.VERBOSE,
    },

    /** @private {!Array<!SelectOption>} */
    logLevelList: {
      type: Array,
      value: [
        {name: 'VERBOSE', value: Severity.VERBOSE},
        {name: 'INFO', value: Severity.INFO},
        {name: 'WARNING', value: Severity.WARNING},
        {name: 'ERROR', value: Severity.ERROR},
      ],
    },
  },

  /** @private {?LogProvider}*/
  logProvider_: null,

  /**
   * When the page is initialized, notify the C++ layer and load in the
   * contents of its log buffer. Initialize WebUI Listeners.
   * @override
   */
  attached() {
    this.logProvider_ = getLogProvider(this.feature);
    this.addWebUIListener(
        this.logProvider_.messageAddedEventName,
        log => this.onLogMessageAdded_(log));
    this.addWebUIListener(
        this.logProvider_.bufferClearedEventName,
        () => this.onWebUILogBufferCleared_());
    this.logProvider_.getLogMessages().then(
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
   * Saves and downloads all javascript logs.
   * @private
   */
  onSaveUnfilteredLogsButtonClicked_() {
    this.onSaveLogsButtonClicked_(false);
  },

  /**
   * Saves and downloads javascript logs that currently appear on the page.
   * @private
   */
  onSaveFilteredLogsButtonClicked_() {
    this.onSaveLogsButtonClicked_(true);
  },

  /**
   * Saves and downloads javascript logs.
   * @param {!boolean} filtered
   * @private
   */
  onSaveLogsButtonClicked_(filtered) {
    let blob;
    if (filtered) {
      blob = new Blob(
          this.filteredLogList_.map(logToSavedString_),
          {type: 'text/plain;charset=utf-8'});
    } else {
      blob = new Blob(
          this.logList_.map(logToSavedString_),
          {type: 'text/plain;charset=utf-8'});
    }
    const url = URL.createObjectURL(blob);

    const anchorElement = document.createElement('a');
    anchorElement.href = url;
    anchorElement.download =
        this.logProvider_.logFilePrefix + new Date().toJSON() + '.txt';
    document.body.appendChild(anchorElement);
    anchorElement.click();

    window.setTimeout(function() {
      document.body.removeChild(anchorElement);
      window.URL.revokeObjectURL(url);
    }, 0);
  },

  /**
   * Adds a log message to the javascript log list displayed. Called from the
   * C++ WebUI handler when a log message is added to the log buffer.
   * @param {!LogMessage} log
   * @private
   */
  onLogMessageAdded_(log) {
    this.push('logList_', log);
    if ((log.text.match(this.currentFilter) ||
         log.file.match(this.currentFilter)) &&
        log.severity >= this.currentSeverity) {
      this.push('filteredLogList_', log);
    }
  },

  addLogFilter() {
    switch (Number(this.$.logLevelSelector.value)) {
      case Severity.VERBOSE:
        this.set(
            'filteredLogList_',
            this.logList_.filter((log) => log.severity >= Severity.VERBOSE));
        this.currentSeverity = Severity.VERBOSE;
        break;
      case Severity.INFO:
        this.set(
            'filteredLogList_',
            this.logList_.filter((log) => log.severity >= Severity.INFO));
        this.currentSeverity = Severity.INFO;
        break;
      case Severity.WARNING:
        this.set(
            'filteredLogList_',
            this.logList_.filter((log) => log.severity >= Severity.WARNING));
        this.currentSeverity = Severity.WARNING;
        break;
      case Severity.ERROR:
        this.set(
            'filteredLogList_',
            this.logList_.filter((log) => log.severity >= Severity.ERROR));
        this.currentSeverity = Severity.ERROR;
        break;
    }

    this.currentFilter = this.$.logSearch.value;
    this.set(
        'filteredLogList_',
        this.filteredLogList_.filter(
            (log) =>
                (log.text.match(this.currentFilter) ||
                 log.file.match(this.currentFilter))));
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
    this.logList_ = logs.concat(this.logList_);
    this.filteredLogList_ = logs.concat(this.filteredLogList_);
  },

  /**
   * Clears the javascript log buffer.
   * @private
   */
  clearLogBuffer_() {
    this.logList_ = [];
    this.filteredLogList_ = [];
  },
});
