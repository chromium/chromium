// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Converts a number in bytes to a string in megabytes split by comma into
 * three digit block.
 * @param {number} bytes The number in bytes.
 * @return {string} Formatted string in megabytes.
 */
function toMegaByteString(bytes) {
  const mb = Math.floor(bytes / (1 << 20));
  return mb.toString().replace(
      /\d+?(?=(\d{3})+$)/g,  // Digit sequence (\d+) followed (?=) by 3n digits.
      function(three_digit_block) {
        return three_digit_block + ',';
      });
}

/**
 * Updates the Drive related Preferences section.
 * @param {Array} preferences List of dictionaries describing preferences.
 */
function updateDriveRelatedPreferences(preferences) {
  const ul = $('drive-related-preferences');
  updateKeyValueList(ul, preferences);
}

/**
 * Updates the Connection Status section.
 * @param {Object} connStatus Dictionary containing connection status.
 */
function updateConnectionStatus(connStatus) {
  $('connection-status').textContent = connStatus['status'];
  $('push-notification-enabled').textContent =
      connStatus['push-notification-enabled'];
}

/**
 * Updates the Path Configurations section.
 * @param {Array} paths List of dictionaries describing paths.
 */
function updatePathConfigurations(paths) {
  const ul = $('path-configurations');
  updateKeyValueList(ul, paths);
}

/**
 * Updates the GCache Contents section.
 * @param {Array} gcacheContents List of dictionaries describing metadata
 * of files and directories under the GCache directory.
 * @param {Object} gcacheSummary Dictionary of summary of GCache.
 */
function updateGCacheContents(gcacheContents, gcacheSummary) {
  const tbody = $('gcache-contents');
  for (let i = 0; i < gcacheContents.length; i++) {
    const entry = gcacheContents[i];
    const tr = document.createElement('tr');

    // Add some suffix based on the type.
    let path = entry.path;
    if (entry.is_directory) {
      path += '/';
    } else if (entry.is_symbolic_link) {
      path += '@';
    }

    tr.appendChild(createElementFromText('td', path));
    tr.appendChild(createElementFromText('td', entry.size));
    tr.appendChild(createElementFromText('td', entry.last_modified));
    tr.appendChild(createElementFromText('td', entry.permission));
    tbody.appendChild(tr);
  }

  $('gcache-summary-total-size').textContent =
      toMegaByteString(gcacheSummary['total_size']);
}

/**
 * Updates the Cache Contents section.
 * @param {Object} cacheEntry Dictionary describing a cache entry.
 * The function is called from the C++ side repeatedly.
 */
function updateCacheContents(cacheEntry) {
  const tr = document.createElement('tr');
  tr.appendChild(createElementFromText('td', cacheEntry.local_id));
  tr.appendChild(createElementFromText('td', cacheEntry.md5));
  tr.appendChild(createElementFromText('td', cacheEntry.is_present));
  tr.appendChild(createElementFromText('td', cacheEntry.is_pinned));
  tr.appendChild(createElementFromText('td', cacheEntry.is_dirty));

  $('cache-contents').appendChild(tr);
}

function updateBulkPinningVisible(enabled) {
  $('bulk-pinning-visible').checked = enabled;
}

function updateVerboseLogging(enabled) {
  $('verbose-logging-toggle').checked = enabled;
}

function updateMirroring(enabled) {
  $('mirroring-toggle').checked = enabled;
}

function updateBulkPinning(enabled) {
  $('bulk-pinning-toggle').checked = enabled;
}

function onBulkPinningProgress(progress) {
  updateBulkPinning(progress.enabled);
  $('bulk-pinning-stage').innerText = progress.stage;
  $('bulk-pinning-free-space').innerText = progress.free_space;
  $('bulk-pinning-required-space').innerText = progress.required_space;
  $('bulk-pinning-bytes-to-pin').innerText = progress.bytes_to_pin;
  $('bulk-pinning-pinned-bytes').innerText = progress.pinned_bytes;
  $('bulk-pinning-pinned-bytes-percent').innerText =
      progress.pinned_bytes_percent;
  $('bulk-pinning-files-to-pin').innerText = progress.files_to_pin;
  $('bulk-pinning-pinned-files').innerText = progress.pinned_files;
  $('bulk-pinning-pinned-files-percent').innerText =
      progress.pinned_files_percent;
  $('bulk-pinning-failed-files').innerText = progress.failed_files;
  $('bulk-pinning-syncing-files').innerText = progress.syncing_files;
  $('bulk-pinning-skipped-items').innerText = progress.skipped_items;
  $('bulk-pinning-listed-items').innerText = progress.listed_items;
  $('bulk-pinning-listed-dirs').innerText = progress.listed_dirs;
  $('bulk-pinning-listed-files').innerText = progress.listed_files;
  $('bulk-pinning-listed-docs').innerText = progress.listed_docs;
  $('bulk-pinning-listed-shortcuts').innerText = progress.listed_shortcuts;
  $('bulk-pinning-active-queries').innerText = progress.active_queries;
  $('bulk-pinning-max-active-queries').innerText = progress.max_active_queries;
  $('bulk-pinning-time-spent-listing-items').innerText =
      progress.time_spent_listing_items;
  $('bulk-pinning-time-spent-pinning-files').innerText =
      progress.time_spent_pinning_files;
  $('bulk-pinning-remaining-time').innerText = progress.remaining_time;
}

function updateStartupArguments(args) {
  $('startup-arguments-input').value = args;
}

/**
 * Updates the Local Storage summary.
 * @param {Object} localStorageSummary Dictionary describing the status of local
 * stogage.
 */
function updateLocalStorageUsage(localStorageSummary) {
  const freeSpaceInMB = toMegaByteString(localStorageSummary.free_space);
  $('local-storage-freespace').innerText = freeSpaceInMB;
}

/**
 * Updates the summary about in-flight operations.
 * @param {Array} inFlightOperations List of dictionaries describing the status
 * of in-flight operations.
 */
function updateInFlightOperations(inFlightOperations) {
  const container = $('in-flight-operations-contents');

  // Reset the table. Remove children in reverse order. Otherwides each
  // existingNodes[i] changes as a side effect of removeChild.
  const existingNodes = container.childNodes;
  for (let i = existingNodes.length - 1; i >= 0; i--) {
    const node = existingNodes[i];
    if (node.className === 'in-flight-operation') {
      container.removeChild(node);
    }
  }

  // Add in-flight operations.
  for (let i = 0; i < inFlightOperations.length; i++) {
    const operation = inFlightOperations[i];
    const tr = document.createElement('tr');
    tr.className = 'in-flight-operation';
    tr.appendChild(createElementFromText('td', operation.id));
    tr.appendChild(createElementFromText('td', operation.type));
    tr.appendChild(createElementFromText('td', operation.file_path));
    tr.appendChild(createElementFromText('td', operation.state));
    let progress = operation.progress_current + '/' + operation.progress_total;
    if (operation.progress_total > 0) {
      const percent =
          operation.progress_current / operation.progress_total * 100;
      progress += ' (' + Math.round(percent) + '%)';
    }
    tr.appendChild(createElementFromText('td', progress));

    container.appendChild(tr);
  }
}

/**
 * Updates the summary about about resource.
 * @param {Object} aboutResource Dictionary describing about resource.
 */
function updateAboutResource(aboutResource) {
  const quotaTotalInMb = toMegaByteString(aboutResource['account-quota-total']);
  const quotaUsedInMb = toMegaByteString(aboutResource['account-quota-used']);

  $('account-quota-info').textContent =
      quotaUsedInMb + ' / ' + quotaTotalInMb + ' (MB)';
  $('account-largest-changestamp-remote').textContent =
      aboutResource['account-largest-changestamp-remote'];
  $('root-resource-id').textContent = aboutResource['root-resource-id'];
}

/*
 * Updates the summary about delta update status.
 * @param {Object} deltaUpdateStatus Dictionary describing delta update status.
 */
function updateDeltaUpdateStatus(deltaUpdateStatus) {
  const itemContainer = $('delta-update-status');
  for (let i = 0; i < deltaUpdateStatus['items'].length; i++) {
    const update = deltaUpdateStatus['items'][i];
    const tr = document.createElement('tr');
    tr.className = 'delta-update';
    tr.appendChild(createElementFromText('td', update.id));
    tr.appendChild(createElementFromText('td', update.root_entry_path));
    const startPageToken = update.start_page_token;
    tr.appendChild(createElementFromText(
        'td',
        startPageToken + (startPageToken ? ' (loaded)' : ' (not loaded)')));
    tr.appendChild(createElementFromText('td', update.last_check_time));
    tr.appendChild(createElementFromText('td', update.last_check_result));
    tr.appendChild(createElementFromText('td', update.refreshing));

    itemContainer.appendChild(tr);
  }
}

/**
 * Updates the event log section.
 * @param {Array} log Array of events.
 */
function updateEventLog(log) {
  const ul = $('event-log');
  updateKeyValueList(ul, log);
}

/**
 * Updates the service log section.
 * @param {Array} log Log lines.
 */
function updateServiceLog(log) {
  const ul = $('service-log');
  updateKeyValueList(ul, log);
}

/**
 * Updates the service log section.
 * @param {Array} log Log lines.
 */
function updateOtherServiceLogsUrl(url) {
  const link = $('other-logs');
  link.setAttribute('href', url);
}

/**
 * Adds a new row to the syncing paths table upon successful completion.
 * @param {string} path The path that was synced.
 * @param {string} status The drive::FileError as a string without the
 *     "FILE_ERROR_" prefix.
 */
function onAddSyncPath(path, status) {
  $('mirroring-path-status').textContent = status;
  if (status !== 'OK') {
    console.error(`Cannot add sync path '${path}': ${status}`);
    return;
  }

  // Avoid adding paths to the table if they already exist.
  if ($(`mirroring-${path}`)) {
    return;
  }

  const newRow = document.createElement('tr');
  newRow.id = `mirroring-${path}`;
  const deleteButton = createElementFromText('button', 'Delete');
  deleteButton.addEventListener('click', function(e) {
    e.preventDefault();
    chrome.send('removeSyncPath', [path]);
  });
  const deleteCell = document.createElement('td');
  deleteCell.appendChild(deleteButton);
  newRow.appendChild(deleteCell);
  const pathCell = createElementFromText('td', path);
  newRow.appendChild(pathCell);
  $('mirror-sync-paths').appendChild(newRow);
}

/**
 * Remove a path from the syncing table.
 * @param {string} path The path that was synced.
 * @param {string} status The drive::FileError as a string without the
 *     "FILE_ERROR_" prefix.
 */
function onRemoveSyncPath(path, status) {
  if (status !== 'OK') {
    console.error(`Cannot remove sync path '${path}': ${status}`);
    return;
  }

  if (!$(`mirroring-${path}`)) {
    return;
  }

  $(`mirroring-${path}`).remove();
}

/**
 * Creates an element named |elementName| containing the content |text|.
 * @param {string} elementName Name of the new element to be created.
 * @param {string} text Text to be contained in the new element.
 * @return {HTMLElement} The newly created HTML element.
 */
function createElementFromText(elementName, text) {
  const element = document.createElement(elementName);
  element.appendChild(document.createTextNode(text));
  return element;
}

/**
 * Updates <ul> element with the given key-value list.
 * @param {HTMLElement} ul <ul> element to be modified.
 * @param {Array} list List of dictionaries containing 'key', 'value' (optional)
 * and 'class' (optional). For each element <li> element with specified class is
 * created.
 */
function updateKeyValueList(ul, list) {
  for (let i = 0; i < list.length; i++) {
    const item = list[i];
    let text = item.key;
    if (item.value !== '') {
      text += ': ' + item.value;
    }

    const li = createElementFromText('li', text);
    if (item.class) {
      li.classList.add(item.class);
    }
    ul.appendChild(li);
  }
}

function updateStartupArgumentsStatus(success) {
  $('arguments-status-text').textContent = (success ? 'success' : 'failed');
}

/**
 * Updates the text next to the 'reset' button to update the status.
 * @param {boolean} success whether or not resetting has succeeded.
 */
function updateResetStatus(success) {
  $('reset-status-text').textContent = (success ? 'success' : 'failed');
}

/**
 * Makes up-to-date table of contents.
 */
function updateToc() {
  const toc = $('toc');
  while (toc.firstChild) {
    toc.removeChild(toc.firstChild);
  }
  const sections = document.getElementsByTagName('section');
  for (let i = 0; i < sections.length; i++) {
    const section = sections[i];
    if (!section.hidden) {
      const header = section.getElementsByTagName('h2')[0];
      const a = createElementFromText('a', header.textContent);
      a.href = '#' + section.id;
      const li = document.createElement('li');
      li.appendChild(a);
      toc.appendChild(li);
    }
  }
}

/**
 * Shows or hides a section.
 * @param {string} section Which section to change.
 * @param {boolean} enabled Whether to enable.
 */
function setSectionEnabled(section, enable) {
  const element = $(section);
  if (element.hidden !== !enable) {
    element.hidden = !enable;
    updateToc();
  }
}

function onZipDone(success) {
  $('button-export-logs').removeAttribute('disabled');
}

document.addEventListener('DOMContentLoaded', () => {
  chrome.send('pageLoaded');

  updateToc();

  $('bulk-pinning-visible')
      .addEventListener(
          'change',
          e => chrome.send('setBulkPinningVisible', [e.target.checked]));

  $('verbose-logging-toggle')
      .addEventListener(
          'change',
          e => chrome.send('setVerboseLoggingEnabled', [e.target.checked]));

  $('mirroring-toggle')
      .addEventListener(
          'change',
          e => chrome.send('setMirroringEnabled', [e.target.checked]));

  $('bulk-pinning-toggle')
      .addEventListener(
          'change',
          e => chrome.send('setBulkPinningEnabled', [e.target.checked]));

  $('startup-arguments-form').addEventListener('submit', e => {
    e.preventDefault();
    $('arguments-status-text').textContent = 'applying...';
    chrome.send('setStartupArguments', [$('startup-arguments-input').value]);
  });

  $('mirror-path-form').addEventListener('submit', e => {
    e.preventDefault();
    $('mirroring-path-status').textContent = 'adding...';
    chrome.send('addSyncPath', [$('mirror-path-input').value]);
  });

  $('button-enable-tracing')
      .addEventListener('click', () => chrome.send('enableTracing'));

  $('button-disable-tracing')
      .addEventListener('click', () => chrome.send('disableTracing'));

  $('button-enable-networking')
      .addEventListener('click', () => chrome.send('enableNetworking'));

  $('button-disable-networking')
      .addEventListener('click', () => chrome.send('disableNetworking'));

  $('button-enable-force-pause-syncing')
      .addEventListener('click', () => chrome.send('enableForcePauseSyncing'));

  $('button-disable-force-pause-syncing')
      .addEventListener('click', () => chrome.send('disableForcePauseSyncing'));

  $('button-dump-account-settings')
      .addEventListener('click', () => chrome.send('dumpAccountSettings'));

  $('button-load-account-settings')
      .addEventListener('click', () => chrome.send('loadAccountSettings'));

  $('button-restart-drive')
      .addEventListener('click', () => chrome.send('restartDrive'));

  $('button-reset-drive-filesystem').addEventListener('click', () => {
    if (window.confirm(
            'Warning: Any local changes not yet uploaded to the Drive server ' +
            'will be lost, continue?')) {
      $('reset-status-text').textContent = 'resetting...';
      chrome.send('resetDriveFileSystem');
    }
  });

  $('button-export-logs').addEventListener('click', () => {
    $('button-export-logs').setAttribute('disabled', 'true');
    chrome.send('zipLogs');
  });

  window.setInterval(() => chrome.send('periodicUpdate'), 1000);
});
