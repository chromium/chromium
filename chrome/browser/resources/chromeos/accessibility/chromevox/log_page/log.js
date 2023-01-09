// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox log page.
 */
import {BackgroundBridge} from '../common/background_bridge.js';
import {BaseLog, LogType, SerializableLog} from '../common/log_types.js';

const FILTER_CLASS = 'log-filter';
const FILTER_CONTAINER_ID = 'logFilters';

/** Class to manage the log page. */
export class LogPage {
  constructor() {
    /**
     * Store whether each log type is enabled.
     * @private {Object<!LogType, boolean>}
     */
    this.isLogEnabled_ = {};

    this.initPage_();
  }

  static async init() {
    LogPage.instance = new LogPage();
    await LogPage.instance.update();
  }

  /**
   * @param {!LogType} type
   * @return {string}
   * @private
   */
  checkboxId_(type) {
    return type + 'Filter';
  }

  /**
   * @param {!LogType} type
   * @private
   */
  createFilterCheckbox_(type) {
    const label = document.createElement('label');
    const input = document.createElement('input');
    input.id = this.checkboxId_(type);
    input.type = 'checkbox';
    input.classList.add(FILTER_CLASS);
    label.appendChild(input);

    const span = document.createElement('span');
    span.textContent = type;
    label.appendChild(span);

    document.getElementById(FILTER_CONTAINER_ID).appendChild(label);
  }

  /** @private */
  initPage_() {
    for (const type of Object.values(LogType)) {
      this.createFilterCheckbox_(type);
    }

    const clearLogButton = document.getElementById('clearLog');
    clearLogButton.onclick = () => this.onClear_();

    // Set whether the checkboxes are enabled/disabled.
    const params = new URLSearchParams(location.search);
    for (const type of Object.values(LogType)) {
      LogPage.setFilterTypeEnabled(type, params.get(type));
    }

    const saveLogButton = document.getElementById('saveLog');
    saveLogButton.onclick = event => this.onSaveLog_(event);

    // Add click listeners to the checkboxes.
    const checkboxes = document.getElementsByClassName(FILTER_CLASS);
    const filterEventListener = function(event) {
      const target = event.target;
      LogPage.setFilterTypeEnabled(
          logTypeFromId(target.id), String(target.checked));
      location.search = LogPage.createUrlParams();
    };
    for (let i = 0; i < checkboxes.length; i++) {
      checkboxes[i].onclick = filterEventListener;
    }
  }

  /** @private */
  async onClear_() {
    await BackgroundBridge.LogStore.clearLog();
    location.reload();
  }

  /**
   * When saveLog button is clicked this function runs.
   * Save the current log appeared in the page as a plain text.
   * @param {Event} event
   * @private
   */
  onSaveLog_(event) {
    let outputText = '';
    const logs = document.querySelectorAll('#logList p');
    for (let i = 0; i < logs.length; i++) {
      const logText = [];
      logText.push(logs[i].querySelector('.log-type-tag').textContent);
      logText.push(logs[i].querySelector('.log-time-tag').textContent);
      logText.push(logs[i].querySelector('.log-text').textContent);
      outputText += logText.join(' ') + '\n';
    }

    const a = document.createElement('a');
    const date = new Date();
    a.download =
        [
          'chromevox_logpage',
          date.getMonth() + 1,
          date.getDate(),
          date.getHours(),
          date.getMinutes(),
          date.getSeconds(),
        ].join('_') +
        '.txt';
    a.href = 'data:text/plain; charset=utf-8,' + encodeURI(outputText);
    a.click();
  }

  /** Update the states of checkboxes and update logs. */
  async update() {
    for (const type of Object.values(LogType)) {
      const element = document.getElementById(checkboxId(type));
      element.checked = this.isLogEnabled_[type];
    }

    const log = await BackgroundBridge.LogStore.getLogs();
    this.updateLog_(log, document.getElementById('logList'));
  }

  /**
   * Updates the log section.
   * @param {Array<!SerializableLog>} log Array of logs to record.
   * @param {Element} div
   * @private
   */
  updateLog_(log, div) {
    for (let i = 0; i < log.length; i++) {
      if (!LogPage.instance.isLogEnabled_[log[i].logType]) {
        continue;
      }

      const p = document.createElement('p');
      const typeName = document.createElement('span');
      typeName.textContent = log[i].logType;
      typeName.className = 'log-type-tag';
      const timeStamp = document.createElement('span');
      timeStamp.textContent = LogPage.formatTimeStamp(log[i].date);
      timeStamp.className = 'log-time-tag';
      /** textWrapper should be in block scope, not function scope. */
      const textWrapper = document.createElement('pre');
      textWrapper.textContent = log[i].value;
      textWrapper.className = 'log-text';

      p.appendChild(typeName);
      p.appendChild(timeStamp);

      /** Add hide tree button when logType is tree. */
      if (log[i].logType === LogType.TREE) {
        const toggle = document.createElement('label');
        const toggleCheckbox = document.createElement('input');
        toggleCheckbox.type = 'checkbox';
        toggleCheckbox.checked = true;
        toggleCheckbox.onclick = function(event) {
          textWrapper.hidden = !event.target.checked;
        };
        const toggleText = document.createElement('span');
        toggleText.textContent = 'show tree';
        toggle.appendChild(toggleCheckbox);
        toggle.appendChild(toggleText);
        p.appendChild(toggle);
      }

      p.appendChild(textWrapper);
      div.appendChild(p);
    }
  }

  /**
   * Update isLogEnabled_. Set true if checked is null.
   * @param {!LogType} type
   * @param {?string} checked
   */
  static setFilterTypeEnabled(type, checked) {
    LogPage.instance.isLogEnabled_[type] =
        (checked === null || checked === String(true));
  }

  /**
   * Create URL parameter based on LogPage.instance.isLogEnabled_.
   * @return {string}
   */
  static createUrlParams() {
    const urlParams = [];
    for (const type of Object.values(LogType)) {
      urlParams.push(type + 'Filter=' + LogPage.instance.isLogEnabled_[type]);
    }
    return '?' + urlParams.join('&');
  }

  /**
   * Format time stamp.
   * In this log, events are dispatched many times in a short time, so
   * milliseconds order time stamp is required.
   * @param {!Date} date
   * @return {!string}
   */
  static formatTimeStamp(date) {
    let time = date.getTime();
    time -= date.getTimezoneOffset() * 1000 * 60;
    let timeStr =
        ('00' + Math.floor(time / 1000 / 60 / 60) % 24).slice(-2) + ':';
    timeStr += ('00' + Math.floor(time / 1000 / 60) % 60).slice(-2) + ':';
    timeStr += ('00' + Math.floor(time / 1000) % 60).slice(-2) + '.';
    timeStr += ('000' + time % 1000).slice(-3);
    return timeStr;
  }
}

/**
 * @param {!LogType} type
 * @return {string}
 */
function checkboxId(type) {
  return type + 'Filter';
}
/**
 * @param {string} id
 * @return {!LogType}
 */
function logTypeFromId(id) {
  const type = id.slice(0, -6);
  if (!Object.values(LogType).includes(type)) {
    throw new Error('Log page checkbox IDs must be a LogType + "Filter"');
  }
  return /** @type {!LogType} */ (type);
}


document.addEventListener('DOMContentLoaded', async function() {
  await LogPage.init();
}, false);

/** @type {LogPage} */
LogPage.instance;
