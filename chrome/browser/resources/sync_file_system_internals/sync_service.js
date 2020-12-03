// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WebUI to monitor the Sync File System Service.
 */

import {addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {TabBox} from 'chrome://resources/js/cr/ui/tabs.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {createElementFromText} from './utils.js';

/**
 * Request Sync Service Status.
 */
function refreshServiceStatus() {
  sendWithPromise('getServiceStatus').then(onGetServiceStatus);
}

/**
 * Called when service status is initially retrieved or updated via events.
 * @param {string} statusString Service status enum as a string.
 */
function onGetServiceStatus(statusString) {
  $('service-status').textContent = statusString;
}

/**
 * Request Google Drive Notification Source. e.g. XMPP or polling.
 */
function refreshNotificationSource() {
  sendWithPromise('getNotificationSource').then(onGetNotificationSource);
}

/**
 * Handles callback from getNotificationSource.
 * @param {string} sourceString Notification source as a string.
 */
function onGetNotificationSource(sourceString) {
  $('notification-source').textContent = sourceString;
}

// Keeps track of the last log event seen so it's not reprinted.
let lastLogEventId = -1;

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
  $('log-entries').innerHTML = trustedTypes.emptyHTML;
}

/**
 * Handles callback from getUpdateLog.
 * @param {!Array<!{
 *   id: number,
 *   logEvent: string,
 *   time: string,
 * }>} logEntries List of dictionaries containing 'id', 'time', 'logEvent'.
 */
function onGetLog(logEntries) {
  const itemContainer = $('log-entries');
  for (let i = 0; i < logEntries.length; i++) {
    const logEntry = logEntries[i];
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
  decorate('tabbox', TabBox);
  $('clear-log-button').addEventListener('click', clearLogs);
  refreshServiceStatus();
  refreshNotificationSource();

  addWebUIListener('service-status-changed', onGetServiceStatus);

  // TODO: Look for a way to push entries to the page when necessary.
  window.setInterval(refreshLog, 1000);
}

document.addEventListener('DOMContentLoaded', main);
