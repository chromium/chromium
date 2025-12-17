// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';


// List of log levels in priority order.
const logLevels: LogLevel[] = ['Debug', 'Event', 'User', 'Error'];

type LogLevel = 'Debug'|'Event'|'User'|'Error';

interface LogEntry {
  event: string;
  file: string;
  level: LogLevel;
  timestampshort: string;
  timestamp: string;
  type: string;
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

  /**
   * Creates a tag for the log level.
   */
  createLevelTag(level: LogLevel): HTMLElement {
    const levelClassName = 'log-level-' + level.toLowerCase();
    const tag = document.createElement('span');
    tag.textContent = level;
    tag.classList.add('level-tag');
    tag.classList.add(levelClassName);
    return tag;
  }

  /**
   * Creates a tag for the log type.
   */
  createTypeTag(type: string): HTMLElement {
    const typeClassName = 'log-type-' + type.toLowerCase();
    const tag = document.createElement('span');
    tag.textContent = type;
    tag.classList.add('type-tag');
    tag.classList.add(typeClassName);
    return tag;
  }

  /**
   * Creates an element that contains the time, the event, the level and
   * the description of the given log entry.
   *
   * @param logEntry An object that represents a single line of log.
   * @return The created p element that represents the log entry, or null if the
   *     entry should be skipped.
   */
  createLogEntryText(logEntry: LogEntry): HTMLElement|null {
    const level = logEntry.level;
    const levelIndex = logLevels.indexOf(level);
    const levelSelectIndex =
        logLevels.indexOf(this.$.logLevelSelect.value as LogLevel);
    if (levelIndex < levelSelectIndex) {
      return null;
    }

    const type = logEntry.type;
    const typeCheckbox = this.shadowRoot.querySelector<HTMLInputElement>(
        `#logType${type.toLowerCase()}`);
    if (typeCheckbox && !typeCheckbox.checked) {
      return null;
    }

    const res = document.createElement('p');
    const textWrapper = document.createElement('span');
    let fileinfo = '';
    if (this.$.logFileinfo.checked) {
      fileinfo = logEntry.file;
    }
    let timestamp = '';
    if (this.$.logTimedetail.checked) {
      timestamp = logEntry.timestamp;
    } else {
      timestamp = logEntry.timestampshort;
    }
    textWrapper.textContent = loadTimeData.getStringF(
        'logEntryFormat', timestamp, fileinfo, logEntry.event);
    res.appendChild(this.createTypeTag(type));
    res.appendChild(this.createLevelTag(level));
    res.appendChild(textWrapper);
    return res;
  }

  /**
   * Creates event log entries.
   *
   * @param logEntries An array of strings that represent log log events in JSON
   *     format.
   */
  createEventLog(logEntries: string[]) {
    this.$.logContainer.textContent = '';
    for (const logEntry of logEntries) {
      const entry = this.createLogEntryText(JSON.parse(logEntry));
      if (entry) {
        this.$.logContainer.appendChild(entry);
      }
    }
  }

  /**
   * Callback function, triggered when the log is received.
   */
  getLogCallback(data: string) {
    const container = this.$.logContainer;
    try {
      this.createEventLog(JSON.parse(data));
      if (container.textContent === '') {
        container.textContent = loadTimeData.getString('logNoEntriesText');
      }
    } catch (e) {
      container.textContent = loadTimeData.getString('logNoEntriesText');
    }
  }

  /**
   * Requests a log update.
   */
  requestLog() {
    sendWithPromise('getLog').then(this.getLogCallback.bind(this));
  }

  clearLog() {
    chrome.send('clearLog');
    this.requestLog();
  }

  getCheckboxes(): NodeListOf<HTMLInputElement> {
    return this.shadowRoot.querySelectorAll(
        '#logCheckboxContainer input[type="checkbox"]');
  }

  clearLogTypes() {
    for (const checkbox of this.getCheckboxes()) {
      checkbox.checked = false;
    }
  }

  /**
   * Sets the checked logging types from the URL parameters.
   */
  setCheckedTypes() {
    const checkedTypesInput =
        new URL(window.location.href).searchParams.get('types');
    if (!checkedTypesInput) {
      return;
    }
    this.clearLogTypes();
    const checkedTypes = checkedTypesInput.toLowerCase().split(',');
    for (let i = 0; i < checkedTypes.length; ++i) {
      const checkbox = this.shadowRoot.querySelector<HTMLInputElement>(
          `#logType${checkedTypes[i]}`);
      if (checkbox) {
        checkbox.checked = true;
      }
    }
  }

  /**
   * Sets refresh rate if the interval is found in the url.
   */
  setRefresh() {
    const interval = new URL(window.location.href).searchParams.get('refresh');
    if (interval) {
      setInterval(this.requestLog.bind(this), parseInt(interval, 10) * 1000);
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    // Debug is the default level to show.
    this.$.logLevelSelect.value = 'Debug';
    this.$.logLevelSelect.onchange = this.requestLog.bind(this);

    // Show all types by default.
    let checkboxes = this.shadowRoot.querySelectorAll<HTMLInputElement>(
        '#logCheckboxContainer input[type="checkbox"]');
    for (const checkbox of checkboxes) {
      checkbox.checked = true;
    }

    this.$.logFileinfo.checked = false;
    this.$.logFileinfo.onclick = this.requestLog.bind(this);
    this.$.logTimedetail.checked = false;
    this.$.logTimedetail.onclick = this.requestLog.bind(this);

    this.$.logRefresh.onclick = this.requestLog.bind(this);
    this.$.logClear.onclick = this.clearLog.bind(this);
    this.$.logClearTypes.onclick = this.clearLogTypes.bind(this);

    checkboxes = this.shadowRoot.querySelectorAll<HTMLInputElement>(
        '#logCheckboxContainer input[type="checkbox"]');
    for (const checkbox of checkboxes) {
      checkbox.onclick = this.requestLog.bind(this);
    }

    this.setRefresh();
    this.setCheckedTypes();
    this.requestLog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'device-log-app': DeviceLogAppElement;
  }
}

customElements.define(DeviceLogAppElement.is, DeviceLogAppElement);
