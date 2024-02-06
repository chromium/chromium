// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {IsLogging, OfflineInternalsBrowserProxy, OfflinePage, SavePageRequest} from './offline_internals_browser_proxy.js';
import {OfflineInternalsBrowserProxyImpl} from './offline_internals_browser_proxy.js';

let offlinePages: OfflinePage[] = [];

let savePageRequests: SavePageRequest[] = [];

const browserProxy: OfflineInternalsBrowserProxy =
    OfflineInternalsBrowserProxyImpl.getInstance();

/**
 * Fill stored pages table.
 * @param pages An array object representing stored offline pages.
 */
function fillStoredPages(pages: OfflinePage[]) {
  const storedPagesTable = getRequiredElement('stored-pages');
  storedPagesTable.textContent = '';

  const template =
      getRequiredElement<HTMLTemplateElement>('stored-pages-table-row');
  const td = template.content.querySelectorAll('td');
  for (let pageIndex = 0; pageIndex < pages.length; pageIndex++) {
    const page = pages[pageIndex]!;
    td[0]!.textContent = (pageIndex + 1).toString();
    const checkbox = td[1]!.querySelector('input');
    assert(checkbox);
    checkbox.setAttribute('value', page.id);

    const link = td[2]!.querySelector('a');
    assert(link);
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

    td[3]!.textContent = page.namespace;
    td[4]!.textContent = (Math.round(Number(page.size) / 1024)).toString();

    const row = document.importNode(template.content, true);
    storedPagesTable.appendChild(row);
  }
  offlinePages = pages;
}

/**
 * Fill requests table.
 * @param requests An array object representing the request queue.
 */
function fillRequestQueue(requests: SavePageRequest[]) {
  const requestQueueTable = getRequiredElement('request-queue');
  requestQueueTable.textContent = '';

  const template =
      getRequiredElement<HTMLTemplateElement>('request-queue-table-row');
  const td = template.content.querySelectorAll('td');
  for (const request of requests) {
    const checkbox = td[0]!.querySelector('input');
    assert(checkbox);
    checkbox.setAttribute('value', request.id);

    td[1]!.textContent = request.onlineUrl;
    td[2]!.textContent = new Date(request.creationTime).toString();
    td[3]!.textContent = request.status;
    td[4]!.textContent = request.requestOrigin;

    const row = document.importNode(template.content, true);
    requestQueueTable.appendChild(row);
  }
  savePageRequests = requests;
}

/**
 * Fills the event logs section.
 * @param logs A list of log strings.
 */
function fillEventLog(logs: string[]) {
  const element = getRequiredElement('logs');
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
    getRequiredElement('current-status').textContent = networkStatus;
  });
  browserProxy.getLimitlessPrefetchingEnabled().then(function(enabled) {
    getRequiredElement<HTMLInputElement>('limitless-prefetching-checkbox')
        .checked = enabled;
  });
  browserProxy.getPrefetchTestingHeaderValue().then(function(value) {
    switch (value) {
      case 'ForceEnable':
        getRequiredElement<HTMLInputElement>('testing-header-enable').checked =
            true;
        break;
      case 'ForceDisable':
        getRequiredElement<HTMLInputElement>('testing-header-disable').checked =
            true;
        break;
      default:
        getRequiredElement<HTMLInputElement>('testing-header-default').checked =
            true;
    }
  });
  refreshLog();
}

/**
 * Callback when pages are deleted.
 * @param status The status of the request.
 */
function pagesDeleted(status: string) {
  getRequiredElement('page-actions-info').textContent = status;
  browserProxy.getStoredPages().then(fillStoredPages);
}

/**
 * Callback when requests are deleted.
 */
function requestsDeleted(status: string) {
  getRequiredElement('request-queue-actions-info').textContent = status;
  browserProxy.getRequestQueue().then(fillRequestQueue);
}

/**
 * Callback for prefetch actions.
 * @param info The result of performing the prefetch actions.
 */
function setPrefetchResult(info: string) {
  getRequiredElement('prefetch-actions-info').textContent = info;
}

/**
 * Error callback for prefetch actions.
 * @param error The error that resulted from the prefetch call.
 */
function prefetchResultError(error: Error|string) {
  const errorText =
      error && (error as Error).message ? (error as Error).message : error;

  getRequiredElement('prefetch-actions-info').textContent =
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

  getRequiredElement<HTMLTextAreaElement>('dump-box').value = json;
  getRequiredElement('dump-info').textContent = '';
  getRequiredElement<HTMLDialogElement>('dump-modal').showModal();
  getRequiredElement<HTMLTextAreaElement>('dump-box').select();
}

function closeDump() {
  getRequiredElement<HTMLDialogElement>('dump-modal').close();
  getRequiredElement<HTMLTextAreaElement>('dump-box').value = '';
}

function copyDump() {
  getRequiredElement<HTMLTextAreaElement>('dump-box').select();
  document.execCommand('copy');
  getRequiredElement('dump-info').textContent = 'Copied to clipboard!';
}

/**
 * Updates the status strings.
 * @param logStatus Status of logging.
 */
function updateLogStatus(logStatus: IsLogging) {
  getRequiredElement<HTMLInputElement>('model-checkbox').checked =
      logStatus.modelIsLogging;
  getRequiredElement<HTMLInputElement>('request-checkbox').checked =
      logStatus.queueIsLogging;
  getRequiredElement<HTMLInputElement>('prefetch-checkbox').checked =
      logStatus.prefetchIsLogging;
}

/**
 * Sets all checkboxes with a specific name to the same checked status as the
 * provided source checkbox.
 * @param source The checkbox controlling the checked status.
 * @param checkboxesName The name identifying the checkboxes to set.
 */
function toggleAllCheckboxes(source: HTMLInputElement, checkboxesName: string) {
  const checkboxes = document.getElementsByName(checkboxesName);
  for (const checkbox of checkboxes) {
    (checkbox as HTMLInputElement).checked = source.checked;
  }
}

/**
 * Return the item ids for the selected checkboxes with a given name.
 * @param checkboxesName The name identifying the checkboxes to query.
 * @return An array of selected ids.
 */
function getSelectedIdsFor(checkboxesName: string): string[] {
  const checkboxes = document.querySelectorAll<HTMLInputElement>(
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
      .then((result: string) => {
        // The delays in these messages should correspond to the scheduling
        // delays defined in PrefetchBackgroundTaskScheduler.java.
        if (getRequiredElement<HTMLInputElement>(
                'limitless-prefetching-checkbox')
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
          el => getRequiredElement<HTMLInputElement>(el).disabled = incognito);

  getRequiredElement('delete-selected-pages').onclick = function() {
    const pageIds = getSelectedIdsFor('stored');
    browserProxy.deleteSelectedPages(pageIds).then(pagesDeleted);
  };
  getRequiredElement('delete-selected-requests').onclick = function() {
    const requestIds = getSelectedIdsFor('requests');
    browserProxy.deleteSelectedRequests(requestIds).then(requestsDeleted);
  };
  getRequiredElement('refresh').onclick = refreshAll;
  getRequiredElement('dump').onclick = dumpAsJson;
  getRequiredElement('close-dump').onclick = closeDump;
  getRequiredElement('copy-to-clipboard').onclick = copyDump;
  getRequiredElement('model-checkbox').onchange = (evt: Event) => {
    browserProxy.setRecordPageModel((evt.target as HTMLInputElement).checked);
  };
  getRequiredElement('request-checkbox').onchange = (evt: Event) => {
    browserProxy.setRecordRequestQueue(
        (evt.target as HTMLInputElement).checked);
  };
  getRequiredElement('prefetch-checkbox').onchange = (evt: Event) => {
    browserProxy.setRecordPrefetchService(
        (evt.target as HTMLInputElement).checked);
  };
  getRequiredElement('refresh-logs').onclick = refreshLog;
  getRequiredElement('add-to-queue').onclick = function() {
    const saveUrls =
        getRequiredElement<HTMLInputElement>('url').value.split(',');
    let counter = saveUrls.length;
    getRequiredElement('save-url-state').textContent = '';
    for (let i = 0; i < saveUrls.length; i++) {
      browserProxy.addToRequestQueue(saveUrls[i]!).then(function(state) {
        if (state) {
          getRequiredElement('save-url-state').textContent +=
              saveUrls[i] + ' has been added to queue.\n';
          getRequiredElement<HTMLInputElement>('url').value = '';
          counter--;
          if (counter === 0) {
            browserProxy.getRequestQueue().then(fillRequestQueue);
          }
        } else {
          getRequiredElement('save-url-state').textContent +=
              saveUrls[i] + ' failed to be added to queue.\n';
        }
      });
    }
  };
  getRequiredElement('schedule-nwake').onclick = function() {
    browserProxy.scheduleNwake()
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  getRequiredElement('cancel-nwake').onclick = function() {
    browserProxy.cancelNwake()
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  getRequiredElement('generate-page-bundle').onclick = function() {
    browserProxy
        .generatePageBundle(
            getRequiredElement<HTMLInputElement>('generate-urls').value)
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  getRequiredElement('get-operation').onclick = function() {
    browserProxy
        .getOperation(
            getRequiredElement<HTMLInputElement>('operation-name').value)
        .then(setPrefetchResult)
        .catch(prefetchResultError);
  };
  getRequiredElement('download-archive').onclick = function() {
    browserProxy.downloadArchive(
        getRequiredElement<HTMLInputElement>('download-name').value);
  };
  getRequiredElement('toggle-all-stored').onclick = function() {
    toggleAllCheckboxes(
        getRequiredElement<HTMLInputElement>('toggle-all-stored'), 'stored');
  };
  getRequiredElement('toggle-all-requests').onclick = function() {
    toggleAllCheckboxes(
        getRequiredElement<HTMLInputElement>('toggle-all-requests'),
        'requests');
  };
  getRequiredElement('limitless-prefetching-checkbox').onchange =
      (evt: Event) => {
        const checkbox = evt.target as HTMLInputElement;
        browserProxy.setLimitlessPrefetchingEnabled(checkbox.checked);
        if (checkbox.checked) {
          ensureBackgroundTaskScheduledWithDelay();
        }
      };
  // Helper for setting prefetch testing header from a radio button.
  const setPrefetchTestingHeader = function(evt: Event) {
    browserProxy.setPrefetchTestingHeaderValue(
        (evt.target as HTMLInputElement).value);
    ensureBackgroundTaskScheduledWithDelay();
  };
  getRequiredElement('testing-header-default').onchange =
      setPrefetchTestingHeader;
  getRequiredElement('testing-header-enable').onchange =
      setPrefetchTestingHeader;
  getRequiredElement('testing-header-disable').onchange =
      setPrefetchTestingHeader;
  if (!incognito) {
    refreshAll();
  }
}

document.addEventListener('DOMContentLoaded', initialize);
