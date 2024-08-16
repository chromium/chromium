// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {$} from 'chrome://resources/js/util.js';

// List of log levels in priority order.
const logLevels = ['Debug', 'Event', 'User', 'Error'];

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
  const levelIndex = logLevels.indexOf(level);
  const levelSelectIndex = logLevels.indexOf($('log-level-select').value);
  if (levelIndex < levelSelectIndex) {
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
    if (container.textContent === '') {
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
  sendWithPromise('getLog').then(data => getLogCallback(data));
};

const clearLog = function() {
  chrome.send('clearLog');
  requestLog();
};

const getCheckboxes = function() {
  return document.querySelectorAll(
      '#log-checkbox-container input[type="checkbox"]');
};

const clearLogTypes = function() {
  const checkboxes = getCheckboxes();
  for (let i = 0; i < checkboxes.length; ++i) {
    checkboxes[i].checked = false;
  }
};

/**
 * Sets the checked logging types from the URL parameters.
 */
const setCheckedTypes = function() {
  const checkedTypesInput = new URL(window.location).searchParams.get('types');
  if (!checkedTypesInput) {
    return;
  }
  clearLogTypes();
  const checkedTypes = checkedTypesInput.toLowerCase().split(',');
  for (let i = 0; i < checkedTypes.length; ++i) {
    const checkbox = document.getElementById('log-type-' + checkedTypes[i]);
    if (checkbox) {
      checkbox.checked = true;
    }
  }
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

// <if expr="chromeos_ash">
const updateOsLink = function() {
  sendWithPromise('isLacrosEnabled').then(function(isLacrosEnabled) {
    $('os-link-container').hidden = !isLacrosEnabled;

    // we hide the header text if Lacros is enabled because the Ash window doesn't
    // have the navigation bar and the hint saying "Add a query param in URL to
    // auto-refresh the page" is no longer helpful for users.
    $('header').hidden = isLacrosEnabled;
  });

  $('os-link-href').onclick = function() {
    chrome.send('openBrowserDeviceLog');
  };
};
// </if>

/**
 * Gets log information from WebUI.
 */
document.addEventListener('DOMContentLoaded', function() {
  // Debug is the default level to show.
  $('log-level-select').value = 'Debug';
  $('log-level-select').onchange = requestLog;

  // Show all types by default.
  let checkboxes = document.querySelectorAll(
      '#log-checkbox-container input[type="checkbox"]');
  for (let i = 0; i < checkboxes.length; ++i) {
    checkboxes[i].checked = true;
  }

  $('log-fileinfo').checked = false;
  $('log-fileinfo').onclick = requestLog;
  $('log-timedetail').checked = false;
  $('log-timedetail').onclick = requestLog;

  $('log-refresh').onclick = requestLog;
  $('log-clear').onclick = clearLog;
  $('log-clear-types').onclick = clearLogTypes;

  checkboxes = document.querySelectorAll(
      '#log-checkbox-container input[type="checkbox"]');
  for (let i = 0; i < checkboxes.length; ++i) {
    checkboxes[i].onclick = requestLog;
  }

  setRefresh();
  setCheckedTypes();
  requestLog();
  // <if expr="chromeos_ash">
  updateOsLink();
  // </if>
});
