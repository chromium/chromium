// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WebUI to monitor the Sync File System Service.
 */
const SyncService = (function() {
  'use strict';

  const SyncService = {};

  /**
   * Request Sync Service Status.
   */
  function refreshServiceStatus() {
    cr.sendWithPromise('getServiceStatus').then(SyncService.onGetServiceStatus);
  }

  /**
   * Called when service status is initially retrieved or updated via events.
   * @param {string} statusString Service status enum as a string.
   */
  SyncService.onGetServiceStatus = function(statusString) {
    $('service-status').textContent = statusString;
  };

  /**
   * Request Google Drive Notification Source. e.g. XMPP or polling.
   */
  function refreshNotificationSource() {
    cr.sendWithPromise('getNotificationSource')
        .then(SyncService.onGetNotificationSource);
  }

  /**
   * Handles callback from getNotificationSource.
   * @param {string} sourceString Notification source as a string.
   */
  SyncService.onGetNotificationSource = function(sourceString) {
    $('notification-source').textContent = sourceString;
  };

  // Keeps track of the last log event seen so it's not reprinted.
  let lastLogEventId = -1;

  /**
   * Request debug log.
   */
  function refreshLog() {
    cr.sendWithPromise('getLog', lastLogEventId).then(SyncService.onGetLog);
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
  SyncService.onGetLog = function(logEntries) {
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
  };

  /**
   * Get initial sync service values and set listeners to get updated values.
   */
  function main() {
    cr.ui.decorate('tabbox', cr.ui.TabBox);
    $('clear-log-button').addEventListener('click', clearLogs);
    refreshServiceStatus();
    refreshNotificationSource();

    cr.addWebUIListener(
        'service-status-changed', SyncService.onGetServiceStatus);

    // TODO: Look for a way to push entries to the page when necessary.
    window.setInterval(refreshLog, 1000);
  }

  document.addEventListener('DOMContentLoaded', main);
  return SyncService;
})();
