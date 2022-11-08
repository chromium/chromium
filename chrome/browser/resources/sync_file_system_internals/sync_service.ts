// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WebUI to monitor the Sync File System Service.
 */

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';

import {createElementFromText} from './utils.js';

/**
 * Request Sync Service Status.
 */
function refreshServiceStatus() {
  sendWithPromise('getServiceStatus').then(onGetServiceStatus);
}

/**
 * Called when service status is initially retrieved or updated via events.
 * @param statusString Service status enum as a string.
 */
function onGetServiceStatus(statusString: string) {
  const serviceStatus = document.querySelector<HTMLElement>('#service-status');
  assert(serviceStatus);
  serviceStatus.textContent = statusString;
}

/**
 * Request Google Drive Notification Source. e.g. XMPP or polling.
 */
function refreshNotificationSource() {
  sendWithPromise('getNotificationSource').then(onGetNotificationSource);
}

/**
 * Handles callback from getNotificationSource.
 * @param sourceString Notification source as a string.
 */
function onGetNotificationSource(sourceString: string) {
  const notificationSource =
      document.querySelector<HTMLElement>('#notification-source');
  assert(notificationSource);
  notificationSource.textContent = sourceString;
}

// Keeps track of the last log event seen so it's not reprinted.
let lastLogEventId: number = -1;

/**
 * Request debug log.
 */
function refreshLog() {
  sendWithPromise('getLog', lastLogEventId).then(onGetLog);
}

/**
 * Clear old logs.
 */
function clearLogs() {
  chrome.send('clearLogs');
  const logEntries = document.querySelector<HTMLElement>('#log-entries');
  assert(logEntries);
  assert(window.trustedTypes);
  logEntries.innerHTML = window.trustedTypes.emptyHTML;
}

/**
 * Handles callback from getUpdateLog.
 */
function onGetLog(
    logEntries: Array<{id: number, logEvent: string, time: string}>) {
  const itemContainer = document.querySelector<HTMLElement>('#log-entries');
  assert(itemContainer);
  for (let i = 0; i < logEntries.length; i++) {
    const logEntry = logEntries[i]!;
    const tr = document.createElement('tr');
    const error = /ERROR/.test(logEntry.logEvent) ? ' error' : '';
    tr.appendChild(
        createElementFromText('td', logEntry.time, {'class': 'log-time'}));
    tr.appendChild(createElementFromText(
        'td', logEntry.logEvent, {'class': 'log-event' + error}));
    itemContainer.appendChild(tr);

    lastLogEventId = logEntry.id;
  }
}

/**
 * Get initial sync service values and set listeners to get updated values.
 */
function main() {
  const tabBox = document.querySelector('cr-tab-box');
  assert(tabBox);
  tabBox.hidden = false;
  const clearButton = document.querySelector<HTMLElement>('#clear-log-button');
  assert(clearButton);
  clearButton.addEventListener('click', clearLogs);
  refreshServiceStatus();
  refreshNotificationSource();

  addWebUiListener('service-status-changed', onGetServiceStatus);

  // TODO: Look for a way to push entries to the page when necessary.
  window.setInterval(refreshLog, 1000);
}

document.addEventListener('DOMContentLoaded', main);
