// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

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

/**
 * Creates a tag for the log level.
 */
function createLevelTag(level: LogLevel): HTMLElement {
  const levelClassName = 'log-level-' + level.toLowerCase();
  const tag = document.createElement('span');
  tag.textContent = level;
  tag.classList.add('level-tag ' + levelClassName);
  return tag;
}

/**
 * Creates a tag for the log type.
 */
function createTypeTag(type: string): HTMLElement {
  const typeClassName = 'log-type-' + type.toLowerCase();
  const tag = document.createElement('span');
  tag.textContent = type;
  tag.classList.add('type-tag ' + typeClassName);
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
function createLogEntryText(logEntry: LogEntry): HTMLElement|null {
  const level = logEntry.level;
  const levelIndex = logLevels.indexOf(level);
  const levelSelectIndex = logLevels.indexOf(
      getRequiredElement<HTMLSelectElement>('#log-level-select').value as
      LogLevel);
  if (levelIndex < levelSelectIndex) {
    return null;
  }

  const type = logEntry.type;
  const typeCheckbox = document.body.querySelector<HTMLInputElement>(
      `#log-type-${type.toLowerCase()}`);
  if (typeCheckbox && typeCheckbox.checked) {
    return null;
  }

  const res = document.createElement('p');
  const textWrapper = document.createElement('span');
  let fileinfo = '';
  if (getRequiredElement<HTMLInputElement>('#log-fileinfo').checked) {
    fileinfo = logEntry.file;
  }
  let timestamp = '';
  if (getRequiredElement<HTMLInputElement>('#log-timedetail').checked) {
    timestamp = logEntry.timestamp;
  } else {
    timestamp = logEntry.timestampshort;
  }
  textWrapper.textContent = loadTimeData.getStringF(
      'logEntryFormat', timestamp, fileinfo, logEntry.event);
  res.appendChild(createTypeTag(type));
  res.appendChild(createLevelTag(level));
  res.appendChild(textWrapper);
  return res;
}

/**
 * Creates event log entries.
 *
 * @param logEntries An array of strings that represent log log events in JSON
 *     format.
 */
function createEventLog(logEntries: string[]) {
  const container = getRequiredElement('#log-container');
  container.textContent = '';
  for (const logEntry of logEntries) {
    const entry = createLogEntryText(JSON.parse(logEntry));
    if (entry) {
      container.appendChild(entry);
    }
  }
}

/**
 * Callback function, triggered when the log is received.
 */
function getLogCallback(data: string) {
  const container = getRequiredElement('#log-container');
  try {
    createEventLog(JSON.parse(data));
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
function requestLog() {
  sendWithPromise('getLog').then(data => getLogCallback(data));
}

function clearLog() {
  chrome.send('clearLog');
  requestLog();
}

function getCheckboxes(): NodeListOf<HTMLInputElement> {
  return document.body.querySelectorAll(
      '#log-checkbox-container input[type="checkbox"]');
}

function clearLogTypes() {
  for (const checkbox of getCheckboxes()) {
    checkbox.checked = false;
  }
}

/**
 * Sets the checked logging types from the URL parameters.
 */
function setCheckedTypes() {
  const checkedTypesInput =
      new URL(window.location.href).searchParams.get('types');
  if (!checkedTypesInput) {
    return;
  }
  clearLogTypes();
  const checkedTypes = checkedTypesInput.toLowerCase().split(',');
  for (let i = 0; i < checkedTypes.length; ++i) {
    const checkbox = document.body.querySelector<HTMLInputElement>(
        `#log-type-${checkedTypes[i]}`);
    if (checkbox) {
      checkbox.checked = true;
    }
  }
}

/**
 * Sets refresh rate if the interval is found in the url.
 */
function setRefresh() {
  const interval = new URL(window.location.href).searchParams.get('refresh');
  if (interval) {
    setInterval(requestLog, parseInt(interval, 10) * 1000);
  }
}

/**
 * Gets log information from WebUI.
 */
document.addEventListener('DOMContentLoaded', function() {
  // Debug is the default level to show.
  getRequiredElement<HTMLSelectElement>('#log-level-select').value = 'Debug';
  getRequiredElement<HTMLSelectElement>('#log-level-select').onchange =
      requestLog;

  // Show all types by default.
  let checkboxes = document.body.querySelectorAll<HTMLInputElement>(
      '#log-checkbox-container input[type="checkbox"]');
  for (const checkbox of checkboxes) {
    checkbox.checked = true;
  }

  getRequiredElement<HTMLInputElement>('#log-fileinfo').checked = false;
  getRequiredElement<HTMLInputElement>('#log-fileinfo').onclick = requestLog;
  getRequiredElement<HTMLInputElement>('#log-timedetail').checked = false;
  getRequiredElement<HTMLInputElement>('#log-timedetail').onclick = requestLog;

  getRequiredElement('#log-refresh').onclick = requestLog;
  getRequiredElement('#log-clear').onclick = clearLog;
  getRequiredElement('#log-clear-types').onclick = clearLogTypes;

  checkboxes = document.body.querySelectorAll<HTMLInputElement>(
      '#log-checkbox-container input[type="checkbox"]');
  for (const checkbox of checkboxes) {
    checkbox.onclick = requestLog;
  }

  setRefresh();
  setCheckedTypes();
  requestLog();
});
