// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './log_object.js';
import './shared_style.css.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyLogsBrowserProxy} from './cross_device_logs_browser_proxy.js';
import {getTemplate} from './logging_tab.html.js';
import type {LogMessage, LogProvider, SelectOption} from './types.js';
import {Severity} from './types.js';

/**
 * Converts log message to string format for saved download file.
 */
function logToSavedString(log: LogMessage): string {
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

const nearbyShareLogProvider: LogProvider = {
  messageAddedEventName: 'log-message-added',
  bufferClearedEventName: 'log-buffer-cleared',
  logFilePrefix: 'nearby_internals_logs_',
  getLogMessages: () => NearbyLogsBrowserProxy.getInstance().getLogMessages(),
};

const quickPairLogProvider: LogProvider = {
  messageAddedEventName: 'quick-pair-log-message-added',
  bufferClearedEventName: 'quick-pair-log-buffer-cleared',
  logFilePrefix: 'fast_pair_logs_',
  getLogMessages: () =>
      NearbyLogsBrowserProxy.getInstance().getQuickPairLogMessages(),
};

/**
 * Gets a log provider instance for a feature.
 */
function getLogProvider(feature: string): LogProvider {
  switch (feature) {
    case 'nearby-share':
      return nearbyShareLogProvider;
    case 'quick-pair':
      return quickPairLogProvider;
    default:
      return quickPairLogProvider;
  }
}


const LoggingTabElementBase = WebUiListenerMixin(PolymerElement);

class LoggingTabElement extends LoggingTabElementBase {
  static get is() {
    return 'logging-tab';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      logList_: {
        type: Array,
        value: () => [],
      },

      filteredLogList_: {
        type: Array,
        value: () => [],
      },

      feature: {
        type: String,
      },

      currentFilter_: {
        type: String,
      },

      currentSeverity_: {
        type: Severity,
        value: Severity.VERBOSE,
      },

      logLevelList_: {
        type: Array,
        value: [
          {name: 'VERBOSE', value: Severity.VERBOSE},
          {name: 'INFO', value: Severity.INFO},
          {name: 'WARNING', value: Severity.WARNING},
          {name: 'ERROR', value: Severity.ERROR},
        ],
      },

    };
  }

  private logList_: LogMessage[];
  private filteredLogList_: LogMessage[];
  private feature: string;
  private currentFilter_: string;
  private currentSeverity_: Severity;
  private logLevelList_: SelectOption[];
  private logProvider_: LogProvider;

  /**
   * When the page is initialized, notify the C++ layer and load in the
   * contents of its log buffer. Initialize WebUI Listeners.
   */
  override connectedCallback() {
    super.connectedCallback();

    this.logProvider_ = getLogProvider(this.feature);
    this.addWebUiListener(
        this.logProvider_.messageAddedEventName,
        (log: LogMessage) => this.onLogMessageAdded_(log));
    this.addWebUiListener(
        this.logProvider_.bufferClearedEventName,
        () => this.onWebUiLogBufferCleared_());
    this.logProvider_.getLogMessages().then(
        logs => this.onGetLogMessages_(logs));
  }

  /**
   * Clears javascript logs displayed, but c++ log buffer remains.
   */
  private onClearLogsButtonClicked_(): void {
    this.clearLogBuffer_();
  }

  /**
   * Saves and downloads all javascript logs.
   */
  private onSaveUnfilteredLogsButtonClicked_(): void {
    this.onSaveLogsButtonClicked_(false);
  }

  /**
   * Saves and downloads javascript logs that currently appear on the page.
   */
  private onSaveFilteredLogsButtonClicked_(): void {
    this.onSaveLogsButtonClicked_(true);
  }

  /**
   * Saves and downloads javascript logs.
   */
  private onSaveLogsButtonClicked_(filtered: boolean): void {
    let blob;
    if (filtered) {
      blob = new Blob(
          this.filteredLogList_.map(logToSavedString),
          {type: 'text/plain;charset=utf-8'});
    } else {
      blob = new Blob(
          this.logList_.map(logToSavedString),
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
  }

  /**
   * Adds a log message to the javascript log list displayed. Called from the
   * C++ WebUI handler when a log message is added to the log buffer.
   */
  private onLogMessageAdded_(log: LogMessage): void {
    this.push('logList_', log);
    if ((log.text.match(this.currentFilter_) ||
         log.file.match(this.currentFilter_)) &&
        log.severity >= this.currentSeverity_) {
      this.push('filteredLogList_', log);
    }
  }

  private addLogFilter_(): void {
    const logLevelSelector: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#logLevelSelector');
    if (logLevelSelector) {
      switch (Number(logLevelSelector.value)) {
        case Severity.VERBOSE:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.VERBOSE));
          this.currentSeverity_ = Severity.VERBOSE;
          break;
        case Severity.INFO:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.INFO));
          this.currentSeverity_ = Severity.INFO;
          break;
        case Severity.WARNING:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.WARNING));
          this.currentSeverity_ = Severity.WARNING;
          break;
        case Severity.ERROR:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.ERROR));
          this.currentSeverity_ = Severity.ERROR;
          break;
      }
    }

    const elem: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#logSearch');
    if (elem) {
      this.currentFilter_ = elem.value;
      this.set(
          'filteredLogList_',
          this.filteredLogList_.filter(
              (log) =>
                  (log.text.match(this.currentFilter_) ||
                   log.file.match(this.currentFilter_))));
    }
  }

  /**
   * Called in response to WebUI handler clearing log buffer.
   */
  private onWebUiLogBufferCleared_(): void {
    this.clearLogBuffer_();
  }

  /**
   * Parses an array of log messages and adds to the javascript list sent in
   * from the initial page load.
   */
  private onGetLogMessages_(logs: LogMessage[]): void {
    this.logList_ = logs.concat(this.logList_);
    this.filteredLogList_ = logs.concat(this.filteredLogList_);
  }

  /**
   * Clears the javascript log buffer.
   */
  private clearLogBuffer_(): void {
    this.logList_ = [];
    this.filteredLogList_ = [];
  }
}

customElements.define(LoggingTabElement.is, LoggingTabElement);
