// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox log page.
 */

import {BackgroundBridge} from '../common/background_bridge.js';
import {BaseLog, LogType, SerializableLog} from '../common/log_types.js';

/**
 * Class to manage the log page.
 */
export class LogPage {
  static async init() {
    /** Create filter checkboxes. */
    for (const type of Object.values(LogType)) {
      const label = document.createElement('label');
      const input = document.createElement('input');
      input.id = type + 'Filter';
      input.type = 'checkbox';
      input.classList.add('log-filter');
      label.appendChild(input);

      const span = document.createElement('span');
      span.textContent = type;
      label.appendChild(span);

      document.getElementById('logFilters').appendChild(label);
    }

    const clearLogButton = document.getElementById('clearLog');
    clearLogButton.onclick = async function(event) {
      await BackgroundBridge.LogStore.clearLog();
      location.reload();
    };

    const params = new URLSearchParams(location.search);
    for (const type of Object.values(LogType)) {
      const typeFilter = type + 'Filter';
      LogPage.setFilterTypeEnabled(typeFilter, params.get(typeFilter));
    }
    const saveLogButton = document.getElementById('saveLog');
    saveLogButton.onclick = LogPage.saveLogEvent;

    const checkboxes = document.getElementsByClassName('log-filter');
    const filterEventListener = function(event) {
      const target = event.target;
      LogPage.setFilterTypeEnabled(target.id, String(target.checked));
      location.search = LogPage.createUrlParams();
    };
    for (let i = 0; i < checkboxes.length; i++) {
      checkboxes[i].onclick = filterEventListener;
    }

    await LogPage.update();
  }

  /**
   * When saveLog button is clicked this function runs.
   * Save the current log appeared in the page as a plain text.
   * @param {Event} event
   */
  static saveLogEvent(event) {
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

  /**
   * Update the states of checkboxes and
   * update logs.
   */
  static async update() {
    for (const type of Object.values(LogType)) {
      const typeFilter = type + 'Filter';
      const element = document.getElementById(typeFilter);
      element.checked = LogPage.urlPrefs_[typeFilter];
    }

    const log = await BackgroundBridge.LogStore.getLogs();
    LogPage.updateLog(log, document.getElementById('logList'));
  }

  /**
   * Updates the log section.
   * @param {Array<!SerializableLog>} log Array of logs to record.
   * @param {Element} div
   */
  static updateLog(log, div) {
    for (let i = 0; i < log.length; i++) {
      if (!LogPage.urlPrefs_[log[i].logType + 'Filter']) {
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
   * Update urlPrefs_. Set true if checked is null.
   * @param {string} typeFilter
   * @param {?string} checked
   */
  static setFilterTypeEnabled(typeFilter, checked) {
    if (checked == null || checked === 'true') {
      LogPage.urlPrefs_[typeFilter] = true;
    } else {
      LogPage.urlPrefs_[typeFilter] = false;
    }
  }

  /**
   * Create URL parameter based on LogPage.urlPrefs_.
   * @return {string}
   */
  static createUrlParams() {
    const urlParams = [];
    for (const type of Object.values(LogType)) {
      const typeFilter = type + 'Filter';
      urlParams.push(typeFilter + '=' + LogPage.urlPrefs_[typeFilter]);
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
 * Store the preferences of filters.
 * @type {Object<string, boolean>}
 * @private
 */
LogPage.urlPrefs_ = {};


document.addEventListener('DOMContentLoaded', async function() {
  await LogPage.init();
}, false);
