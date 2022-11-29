// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {IsLogging, OfflineInternalsBrowserProxy, OfflineInternalsBrowserProxyImpl, OfflinePage, SavePageRequest} from './offline_internals_browser_proxy.js';

/** @type {!Array<OfflinePage>} */
let offlinePages = [];

/** @type {!Array<SavePageRequest>} */
let savePageRequests = [];

/** @type {!OfflineInternalsBrowserProxy} */
const browserProxy = OfflineInternalsBrowserProxyImpl.getInstance();

/**
 * Fill stored pages table.
 * @param {!Array<OfflinePage>} pages An array object representing
 *     stored offline pages.
 */
function fillStoredPages(pages) {
  const storedPagesTable = document.body.querySelector('#stored-pages');
  storedPagesTable.textContent = '';

  const template = document.body.querySelector('#stored-pages-table-row');
  const td = template.content.querySelectorAll('td');
  for (let pageIndex = 0; pageIndex < pages.length; pageIndex++) {
    const page = pages[pageIndex];
    td[0].textContent = pageIndex + 1;
    const checkbox = td[1].querySelector('input');
    checkbox.setAttribute('value', page.id);

    const link = td[2].querySelector('a');
    link.setAttribute('href', page.onlineUrl);
    const maxUrlCharsPerLine = 50;
    if (page.onlineUrl.length > maxUrlCharsPerLine) {
      link.textContent = '';
      for (let i = 0; i < page.onlineUrl.length; i += maxUrlCharsPerLine) {
        link.textContent += page.onlineUrl.slice(i, i + maxUrlCharsPerLine);
        link.textContent += '\r\n';
      }
    } else {
      link.textContent = page.onlineUrl;
    }

    td[3].textContent = page.namespace;
    td[4].textContent = Math.round(page.size / 1024);

    const row = document.importNode(template.content, true);
    storedPagesTable.appendChild(row);
  }
  offlinePages = pages;
}

/**
 * Fill requests table.
 * @param {!Array<SavePageRequest>} requests An array object representing
 *     the request queue.
 */
function fillRequestQueue(requests) {
  const requestQueueTable = document.body.querySelector('#request-queue');
  requestQueueTable.textContent = '';

  const template = document.body.querySelector('#request-queue-table-row');
  const td = template.content.querySelectorAll('td');
  for (const request of requests) {
    const checkbox = td[0].querySelector('input');
    checkbox.setAttribute('value', request.id);

    td[1].textContent = request.onlineUrl;
    td[2].textContent = new Date(request.creationTime);
    td[3].textContent = request.status;
    td[4].textContent = request.requestOrigin;

    const row = document.importNode(template.content, true);
    requestQueueTable.appendChild(row);
  }
  savePageRequests = requests;
}

/**
 * Fills the event logs section.
 * @param {!Array<string>} logs A list of log strings.
 */
function fillEventLog(logs) {
  const element = document.body.querySelector('#logs');
  element.textContent = '';
  for (const log of logs) {
    const logItem = document.createElement('li');
    logItem.textContent = log;
    element.appendChild(logItem);
  }
}

/**
 * Refresh all displayed information.
 */
function refreshAll() {
  browserProxy.getStoredPages().then(fillStoredPages);
  browserProxy.getRequestQueue().then(fillRequestQueue);
  browserProxy.getNetworkStatus().then(function(networkStatus) {
    document.body.querySelector('#current-status').textContent = networkStatus;
  });
  browserProxy.getLimitlessPrefetchingEnabled().then(function(enabled) {
    document.body.querySelector('#limitless-prefetching-checkbox').checked =
        enabled;
  });
  browserProxy.getPrefetchTestingHeaderValue().then(function(value) {
    switch (value) {
      case 'ForceEnable':
        document.body.querySelector('#testing-header-enable').checked = true;
        break;
      case 'ForceDisable':
        document.body.querySelector('#testing-header-disable').checked = true;
        break;
      default:
        document.body.querySelector('#testing-header-default').checked = true;
    }
  });
  refreshLog();
}

/**
 * Callback when pages are deleted.
 * @param {string} status The status of the request.
 */
function pagesDeleted(status) {
  document.body.querySelector('#page-actions-info').textContent = status;
  browserProxy.getStoredPages().then(fillStoredPages);
}

/**
 * Callback when requests are deleted.
 */
function requestsDeleted(status) {
  document.body.querySelector('#request-queue-actions-info').textContent =
      status;
  browserProxy.getRequestQueue().then(fillRequestQueue);
}

/**
 * Callback for prefetch actions.
 * @param {string} info The result of performing the prefetch actions.
 */
function setPrefetchResult(info) {
  document.body.querySelector('#prefetch-actions-info').textContent = info;
}

/**
 * Error callback for prefetch actions.
 * @param {*} error The error that resulted from the prefetch call.
 */
function prefetchResultError(error) {
  const errorText = error && error.message ? error.message : error;

  document.body.querySelector('#prefetch-actions-info').textContent =
      'Error: ' + errorText;
}

/**
 * Downloads all the stored page and request queue information into a file.
 * Also translates all the fields representing datetime into human-readable
 * date strings.
 * TODO(chili): Create a CSV writer that can abstract out the line joining.
 */
function dumpAsJson() {
  const json = JSON.stringify(
      {offlinePages: offlinePages, savePageRequests: savePageRequests},
      function(key, value) {
        return key.endsWith('Time') ? new Date(value).toString() : value;
      },
      2);

  document.body.querySelector('#dump-box').value = json;
  document.body.querySelector('#dump-info').textContent = '';
  document.body.querySelector('#dump-modal').showModal();
  document.body.querySelector('#dump-box').select();
}

function closeDump() {
  document.body.querySelector('#dump-modal').close();
  document.body.querySelector('#dump-box').value = '';
}

function copyDump() {
  document.body.querySelector('#dump-box').select();
  document.execCommand('copy');
  document.body.querySelector('#dump-info').textContent =
      'Copied to clipboard!';
}

/**
 * Updates the status strings.
 * @param {!IsLogging} logStatus Status of logging.
 */
function updateLogStatus(logStatus) {
  document.body.querySelector('#model-checkbox').checked =
      logStatus.modelIsLogging;
  document.body.querySelector('#request-checkbox').checked =
      logStatus.queueIsLogging;
  document.body.querySelector('#prefetch-checkbox').checked =
      logStatus.prefetchIsLogging;
}

/**
 * Sets all checkboxes with a specific name to the same checked status as the
 * provided source checkbox.
 * @param {HTMLElement} source The checkbox controlling the checked
 *     status.
 * @param {string} checkboxesName The name identifying the checkboxes to set.
 */
function toggleAllCheckboxes(source, checkboxesName) {
  const checkboxes = document.getElementsByName(checkboxesName);
  for (const checkbox of checkboxes) {
    checkbox.checked = source.checked;
  }
}

/**
 * Return the item ids for the selected checkboxes with a given name.
 * @param {string} checkboxesName The name identifying the checkboxes to
 *     query.
 * @return {!Array<string>} An array of selected ids.
 */
function getSelectedIdsFor(checkboxesName) {
  const checkboxes = document.querySelectorAll(
      `input[type="checkbox"][name="${checkboxesName}"]:checked`);
  return Array.from(checkboxes).map(c => c.value);
}

/**
 * Refreshes the logs.
 */
function refreshLog() {
  browserProxy.getEventLogs().then(fillEventLog);
  browserProxy.getLoggingState().then(updateLogStatus);
}

/**
 * Calls scheduleNwake and indicates how long the scheduled delay will be.
 */
function ensureBackgroundTaskScheduledWithDelay() {
  browserProxy.scheduleNwake()
      .then((result) => {
        // The delays in these messages should correspond to the scheduling
        // delays defined in PrefetchBackgroundTaskScheduler.java.
        if (document.body.querySelector('#limitless-prefetching-checkbox')
                .checked) {
          setPrefetchResult(
              result +
              ' (Limitless mode enabled; background task scheduled to run' +
              ' in a few seconds.)');
        } else {
          setPrefetchResult(
              result +
              ' (Limitless mode disabled; background task scheduled to run' +
              ' in several minutes.)');
        }
      })
      .catch(prefetchResultError);
}

function initialize() {
  const incognito = loadTimeData.getBoolean('isIncognito');
  ['delete-selected-pages', 'delete-selected-requests', 'model-checkbox',
   'request-checkbox', 'refresh']
      .forEach(
          el => document.body.querySelector(`#${el}`).disabled = incognito);

  document.body.querySelector('#delete-selected-pages').onclick = function() {
    const pageIds = getSelectedIdsFor('stored');
    browserProxy.deleteSelectedPages(pageIds).then(pagesDeleted);
  };
  document.body.querySelector('#delete-selected-requests').onclick =
      function() {
    const requestIds = getSelectedIdsFor('requests');
    browserProxy.deleteSelectedRequests(requestIds).then(requestsDeleted);
  };
  document.body.querySelector('#refresh').onclick = refreshAll;
  document.body.querySelector('#dump').onclick = dumpAsJson;
  document.body.querySelector('#close-dump').onclick = closeDump;
  document.body.querySelector('#copy-to-clipboard').onclick = copyDump;
  document.body.querySelector('#model-checkbox').onchange = (evt) => {
    browserProxy.setRecordPageModel(evt.target.checked);
  };
  document.body.querySelector('#request-checkbox').onchange = (evt) => {
    browserProxy.setRecordRequestQueue(evt.target.checked);
  };
  document.body.querySelector('#prefetch-checkbox').onchange = (evt) => {
    browserProxy.setRecordPrefetchService(evt.target.checked);
  };
  document.body.querySelector('#refresh-logs').onclick = refreshLog;
  document.body.querySelector('#add-to-queue').onclick = function() {
    const saveUrls = document.body.querySelector('#url').value.split(',');
    let counter = saveUrls.length;
    document.body.querySelector('#save-url-state').textContent = '';
    for (let i = 0; i < saveUrls.length; i++) {
      browserProxy.addToRequestQueue(saveUrls[i]).then(function(state) {
        if (state) {
          document.body.querySelector('#save-url-state').textContent +=
              saveUrls[i] + ' has been added to queue.\n';
          document.body.querySelector('#url').value = '';
          counter--;
          if (counter === 0) {
            browserProxy.getRequestQueue().then(fillRequestQueue);
          }
        } else {
          document.body.querySelector('#save-url-state').textContent +=
              saveUrls[i] + ' failed to be added to queue.\n';
        }
      });
    }
  };
  document.body.querySelector('#schedule-nwake').onclick = function() {
    browserProxy.scheduleNwake()
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  document.body.querySelector('#cancel-nwake').onclick = function() {
    browserProxy.cancelNwake()
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  document.body.querySelector('#generate-page-bundle').onclick = function() {
    browserProxy
        .generatePageBundle(document.body.querySelector('#generate-urls').value)
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  document.body.querySelector('#get-operation').onclick = function() {
    browserProxy
        .getOperation(document.body.querySelector('#operation-name').value)
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  document.body.querySelector('#download-archive').onclick = function() {
    browserProxy.downloadArchive(
        document.body.querySelector('#download-name').value);
  };
  document.body.querySelector('#toggle-all-stored').onclick = function() {
    toggleAllCheckboxes(
        /** @type {!HTMLElement} */ (
            document.body.querySelector('#toggle-all-stored')),
        'stored');
  };
  document.body.querySelector('#toggle-all-requests').onclick = function() {
    toggleAllCheckboxes(
        /** @type {!HTMLElement} */ (
            document.body.querySelector('#toggle-all-requests')),
        'requests');
  };
  document.body.querySelector('#limitless-prefetching-checkbox').onchange =
      (evt) => {
        browserProxy.setLimitlessPrefetchingEnabled(evt.target.checked);
        if (evt.target.checked) {
          ensureBackgroundTaskScheduledWithDelay();
        }
      };
  // Helper for setting prefetch testing header from a radio button.
  const setPrefetchTestingHeader = function(evt) {
    browserProxy.setPrefetchTestingHeaderValue(evt.target.value);
    ensureBackgroundTaskScheduledWithDelay();
  };
  document.body.querySelector('#testing-header-default').onchange =
      setPrefetchTestingHeader;
  document.body.querySelector('#testing-header-enable').onchange =
      setPrefetchTestingHeader;
  document.body.querySelector('#testing-header-disable').onchange =
      setPrefetchTestingHeader;
  if (!incognito) {
    refreshAll();
  }
}

document.addEventListener('DOMContentLoaded', initialize);
