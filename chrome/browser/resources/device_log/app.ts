// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserProxy, LogEntry} from './browser_proxy.js';
import {BrowserProxyImpl, LogLevel} from './browser_proxy.js';

// List of log levels in priority order.
const logLevels: LogLevel[] =
    [LogLevel.DEBUG, LogLevel.EVENT, LogLevel.USER, LogLevel.ERROR];

// Values must match the strings provided by the backend, case insensitive.
// LINT.IfChange
const logTypes: string[] = [
  'Bluetooth',
  'Camera',
  'Display',
  'Extensions',
  'Fido',
  'Firmware',
  'Geolocation',
  'Hid',
  'Login',
  'Memory',
  'Network',
  'Power',
  'Printer',
  'Serial',
  'Usb',
];
// LINT.ThenChange(/components/device_event_log/device_event_log_impl.cc)

interface EventType {
  type: string;
  label: string;
  enabled: boolean;
}

export interface DeviceLogAppElement {
  $: {
    logClear: HTMLElement,
    logClearTypes: HTMLElement,
    logContainer: HTMLElement,
    logFileinfo: HTMLInputElement,
    logLevelSelect: HTMLSelectElement,
    logRefresh: HTMLElement,
    logTimedetail: HTMLInputElement,
  };
}

export class DeviceLogAppElement extends CrLitElement {
  static get is() {
    return 'device-log-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      eventTypes_: {type: Array},
      filteredLogs_: {type: Array},
      logFileInfo_: {type: Boolean},
      logTimeDetail_: {type: Boolean},
      logs_: {type: Array},
      selectedLogLevel_: {type: String},
    };
  }

  protected accessor eventTypes_: EventType[];
  protected accessor filteredLogs_: LogEntry[] = [];
  protected accessor logFileInfo_: boolean = false;
  protected accessor logTimeDetail_: boolean = false;
  protected accessor logs_: LogEntry[] = [];
  protected accessor selectedLogLevel_: LogLevel = LogLevel.DEBUG;

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  constructor() {
    super();
    const checkedTypesInput = new URL(window.location.href)
                                  .searchParams.get('types')
                                  ?.toLowerCase()
                                  ?.split(',');
    this.eventTypes_ = logTypes.map(type => {
      return {
        type,
        label: loadTimeData.getString(`logType${type}Text`),
        enabled: !checkedTypesInput ||
            checkedTypesInput.includes(type.toLowerCase()),
      };
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setRefresh_();
    this.onLogRefreshClick_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedLogLevel_') ||
        changedPrivateProperties.has('eventTypes_') ||
        changedPrivateProperties.has('logs_')) {
      const enabledEvents =
          new Set(this.eventTypes_.filter(type => type.enabled)
                      .map(type => type.type.toLowerCase()));
      const selectedLogLevelIndex = logLevels.indexOf(this.selectedLogLevel_);
      this.filteredLogs_ = this.logs_.filter(log => {
        return enabledEvents.has(log.type.toLowerCase()) &&
            logLevels.indexOf(log.level) >= selectedLogLevelIndex;
      });
    }
  }

  protected getTextForLogEntry_(log: LogEntry): string {
    const timestamp = this.logTimeDetail_ ? log.timestamp : log.timestampshort;
    const fileInfo = this.logFileInfo_ ? log.file : '';
    return loadTimeData.getStringF(
        'logEntryFormat', timestamp, fileInfo, log.event);
  }

  protected onLogTypeChange_(event: Event) {
    const target = event.target as HTMLInputElement;
    this.eventTypes_ = this.eventTypes_.map(type => {
      return type.type === target.value ? {...type, enabled: target.checked} :
                                          type;
    });
  }

  protected onLogLevelSelectChange_(event: Event) {
    const target = event.target as HTMLSelectElement;
    this.selectedLogLevel_ = target.value as LogLevel;
  }

  protected onLogFileinfoChange_(event: Event) {
    const target = event.target as HTMLInputElement;
    this.logFileInfo_ = target.checked;
  }

  protected onLogTimedetailChange_(event: Event) {
    const target = event.target as HTMLInputElement;
    this.logTimeDetail_ = target.checked;
  }

  protected onLogClearTypesClick_() {
    this.eventTypes_ =
        this.eventTypes_.map(type => ({...type, enabled: false}));
  }

  /**
   * Requests a log update.
   */
  protected async onLogRefreshClick_() {
    const logs = await this.browserProxy_.getLog();
    // The backend returns a JSON-encoded string of JSON-encoded log entries.
    const logStrings = JSON.parse(logs);
    this.logs_ = logStrings.map(JSON.parse);
  }

  /**
   * Clears the log and queues an update.
   */
  protected onLogClearClick_() {
    this.browserProxy_.clearLog();
    this.logs_ = [];
  }

  /**
   * Sets refresh rate if the interval is found in the url.
   */
  private setRefresh_() {
    const interval = new URL(window.location.href).searchParams.get('refresh');
    if (interval) {
      setInterval(
          this.onLogRefreshClick_.bind(this), parseInt(interval, 10) * 1000);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'device-log-app': DeviceLogAppElement;
  }
}

customElements.define(DeviceLogAppElement.is, DeviceLogAppElement);
