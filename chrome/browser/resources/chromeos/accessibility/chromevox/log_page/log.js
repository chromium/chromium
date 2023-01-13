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
const LOG_LIST_ID = 'logList';

/** Class to manage the log page. */
export class LogPage {
  constructor() {
    this.initPage_();
  }

  static async init() {
    LogPage.instance = new LogPage();
    await LogPage.instance.update();
  }

  /**
   * @param {!SerializableLog} log
   * @private
   */
  addLogToPage_(log) {
    const div = document.getElementById(LOG_LIST_ID);
    const p = document.createElement('p');

    const typeName = document.createElement('span');
    typeName.textContent = log.logType;
    typeName.className = 'log-type-tag';
    p.appendChild(typeName);

    const timeStamp = document.createElement('span');
    timeStamp.textContent = this.formatTimeStamp_(log.date);
    timeStamp.className = 'log-time-tag';
    p.appendChild(timeStamp);

    /** Add hide tree button when logType is tree. */
    if (log.logType === LogType.TREE) {
      const toggle = document.createElement('label');
      const toggleCheckbox = document.createElement('input');
      toggleCheckbox.type = 'checkbox';
      toggleCheckbox.checked = true;
      toggleCheckbox.onclick = event => textWrapper.hidden =
          !event.target.checked;

      const toggleText = document.createElement('span');
      toggleText.textContent = 'show tree';
      toggle.appendChild(toggleCheckbox);
      toggle.appendChild(toggleText);
      p.appendChild(toggle);
    }

    /** textWrapper should be in block scope, not function scope. */
    const textWrapper = document.createElement('pre');
    textWrapper.textContent = log.value;
    textWrapper.className = 'log-text';
    p.appendChild(textWrapper);

    div.appendChild(p);
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
   * @param {boolean} checked
   * @private
   */
  createFilterCheckbox_(type, checked) {
    const label = document.createElement('label');
    const input = document.createElement('input');
    input.id = this.checkboxId_(type);
    input.type = 'checkbox';
    input.classList.add(FILTER_CLASS);
    input.checked = checked;
    input.addEventListener('click', () => this.updateUrlParams_());
    label.appendChild(input);

    const span = document.createElement('span');
    span.textContent = type;
    label.appendChild(span);

    document.getElementById(FILTER_CONTAINER_ID).appendChild(label);
  }

  /** @private */
  getDownloadFileName_() {
    const date = new Date();
    return [
      'chromevox_logpage',
      date.getMonth() + 1,
      date.getDate(),
      date.getHours(),
      date.getMinutes(),
      date.getSeconds(),
    ].join('_') +
        '.txt';
  }

  /** @private */
  initPage_() {
    const params = new URLSearchParams(location.search);
    for (const type of Object.values(LogType)) {
      const enabled =
          (params.get(type) === String(true) || params.get(type) === null);
      this.createFilterCheckbox_(type, enabled);
    }

    const clearLogButton = document.getElementById('clearLog');
    clearLogButton.onclick = () => this.onClear_();

    const saveLogButton = document.getElementById('saveLog');
    saveLogButton.onclick = event => this.onSave_(event);
  }

  /**
   * @param {!LogType} type
   * @private
   */
  isEnabled_(type) {
    return document.getElementById(this.checkboxId_(type)).checked;
  }

  /**
   * @param {Element} log
   * @private
   */
  logToString_(log) {
    const logText = [];
    logText.push(log.querySelector('.log-type-tag').textContent);
    logText.push(log.querySelector('.log-time-tag').textContent);
    logText.push(log.querySelector('.log-text').textContent);
    return logText.join(' ');
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
  onSave_(event) {
    let outputText = '';
    const logs = document.querySelectorAll('#logList p');
    for (const log of logs) {
      outputText += this.logToString_(log) + '\n';
    }

    const a = document.createElement('a');
    a.download = this.getDownloadFileName_();
    a.href = 'data:text/plain; charset=utf-8,' + encodeURI(outputText);
    a.click();
  }

  /** Update the logs. */
  async update() {
    const logs = await BackgroundBridge.LogStore.getLogs();
    if (!logs) {
      return;
    }

    for (const log of logs) {
      if (this.isEnabled_(log.logType)) {
        this.addLogToPage_(log);
      }
    }
  }

  /**
   * Update the URL parameter based on the checkboxes.
   * @private
   */
  updateUrlParams_() {
    const urlParams = [];
    for (const type of Object.values(LogType)) {
      urlParams.push(type + 'Filter=' + LogPage.instance.isEnabled_(type));
    }
    location.search = '?' + urlParams.join('&');
  }

  /**
   * Format time stamp.
   * In this log, events are dispatched many times in a short time, so
   * milliseconds order time stamp is required.
   * @param {!Date} date
   * @return {!string}
   * @private
   */
  formatTimeStamp_(date) {
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
