// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DeviceLogUI = (function() {
  'use strict';

  /**
   * Creates a tag for the log level.
   *
   * @param {string} level A string that represents log level.
   * @return {HTMLSpanElement} The created span element.
   */
  const createLevelTag = function(level) {
    const levelClassName = 'log-level-' + level.toLowerCase();
    const tag = document.createElement('span');
    tag.textContent = level;
    tag.className = 'level-tag ' + levelClassName;
    return tag;
  };

  /**
   * Creates a tag for the log type.
   *
   * @param {string} level A string that represents log type.
   * @return {HTMLSpanElement} The created span element.
   */
  const createTypeTag = function(type) {
    const typeClassName = 'log-type-' + type.toLowerCase();
    const tag = document.createElement('span');
    tag.textContent = type;
    tag.className = 'type-tag ' + typeClassName;
    return tag;
  };

  /**
   * Creates an element that contains the time, the event, the level and
   * the description of the given log entry.
   *
   * @param {Object} logEntry An object that represents a single line of log.
   * @return {?HTMLParagraphElement} The created p element that represents
   *     the log entry, or null if the entry should be skipped.
   */
  const createLogEntryText = function(logEntry) {
    const level = logEntry['level'];
    const levelCheckbox = 'log-level-' + level.toLowerCase();
    if ($(levelCheckbox) && !$(levelCheckbox).checked) {
      return null;
    }

    const type = logEntry['type'];
    const typeCheckbox = 'log-type-' + type.toLowerCase();
    if ($(typeCheckbox) && !$(typeCheckbox).checked) {
      return null;
    }

    const res = document.createElement('p');
    const textWrapper = document.createElement('span');
    let fileinfo = '';
    if ($('log-fileinfo').checked) {
      fileinfo = logEntry['file'];
    }
    let timestamp = '';
    if ($('log-timedetail').checked) {
      timestamp = logEntry['timestamp'];
    } else {
      timestamp = logEntry['timestampshort'];
    }
    textWrapper.textContent = loadTimeData.getStringF(
        'logEntryFormat', timestamp, fileinfo, logEntry['event']);
    res.appendChild(createTypeTag(type));
    res.appendChild(createLevelTag(level));
    res.appendChild(textWrapper);
    return res;
  };

  /**
   * Creates event log entries.
   *
   * @param {Array<string>} logEntries An array of strings that represent log
   *     log events in JSON format.
   */
  const createEventLog = function(logEntries) {
    const container = $('log-container');
    container.textContent = '';
    for (let i = 0; i < logEntries.length; ++i) {
      const entry = createLogEntryText(JSON.parse(logEntries[i]));
      if (entry) {
        container.appendChild(entry);
      }
    }
  };

  /**
   * Callback function, triggered when the log is received.
   *
   * @param {Object} data A JSON structure of event log entries.
   */
  const getLogCallback = function(data) {
    const container = $('log-container');
    try {
      createEventLog(JSON.parse(data));
      if (container.textContent == '') {
        container.textContent = loadTimeData.getString('logNoEntriesText');
      }
    } catch (e) {
      container.textContent = loadTimeData.getString('logNoEntriesText');
    }
  };

  /**
   * Requests a log update.
   */
  const requestLog = function() {
    chrome.send('DeviceLog.getLog');
  };

  const clearLog = function() {
    chrome.send('DeviceLog.clearLog');
    requestLog();
  };

  /**
   * Sets refresh rate if the interval is found in the url.
   */
  const setRefresh = function() {
    const interval = new URL(window.location).searchParams.get('refresh');
    if (interval) {
      setInterval(requestLog, parseInt(interval, 10) * 1000);
    }
  };

  /**
   * Gets log information from WebUI.
   */
  document.addEventListener('DOMContentLoaded', function() {
    // Show all levels except 'debug' by default.
    $('log-level-error').checked = true;
    $('log-level-user').checked = true;
    $('log-level-event').checked = true;
    $('log-level-debug').checked = false;

    // Show all types by default.
    let checkboxes = document.querySelectorAll(
        '#log-checkbox-container input[type="checkbox"][id*="log-type"]');
    for (let i = 0; i < checkboxes.length; ++i) {
      checkboxes[i].checked = true;
    }

    $('log-fileinfo').checked = false;
    $('log-timedetail').checked = false;

    $('log-refresh').onclick = requestLog;
    $('log-clear').onclick = clearLog;
    checkboxes = document.querySelectorAll(
        '#log-checkbox-container input[type="checkbox"]');
    for (let i = 0; i < checkboxes.length; ++i) {
      checkboxes[i].onclick = requestLog;
    }

    setRefresh();
    requestLog();
  });

  return {getLogCallback: getLogCallback};
})();
