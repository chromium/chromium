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
  var mb = Math.floor(bytes / (1 << 20));
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
  var ul = $('drive-related-preferences');
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
  var ul = $('path-configurations');
  updateKeyValueList(ul, paths);
}

/**
 * Updates the GCache Contents section.
 * @param {Array} gcacheContents List of dictionaries describing metadata
 * of files and directories under the GCache directory.
 * @param {Object} gcacheSummary Dictionary of summary of GCache.
 */
function updateGCacheContents(gcacheContents, gcacheSummary) {
  var tbody = $('gcache-contents');
  for (var i = 0; i < gcacheContents.length; i++) {
    var entry = gcacheContents[i];
    var tr = document.createElement('tr');

    // Add some suffix based on the type.
    var path = entry.path;
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
  var tr = document.createElement('tr');
  tr.appendChild(createElementFromText('td', cacheEntry.local_id));
  tr.appendChild(createElementFromText('td', cacheEntry.md5));
  tr.appendChild(createElementFromText('td', cacheEntry.is_present));
  tr.appendChild(createElementFromText('td', cacheEntry.is_pinned));
  tr.appendChild(createElementFromText('td', cacheEntry.is_dirty));

  $('cache-contents').appendChild(tr);
}

function updateVerboseLogging(enabled) {
  $('verbose-logging-toggle').checked = enabled;
}

function updateMirroring(enabled) {
  $('mirroring-toggle').checked = enabled;
}

function updateBulkPinning(enabled) {
  $('bulk-pinning-toggle').checked = enabled;
  if (enabled) {
    return;
  }
  $('bulk-pinning-setup-stage').innerText = 'Unknown';
  $('bulk-pinning-available-disk-space').innerText = 'Unknown';
  $('bulk-pinning-required-disk-space').innerText = 'Unknown';
  $('bulk-pinning-pinned-disk-space').innerText = 'Unknown';
  $('bulk-pinning-progress').removeAttribute('max');
  $('bulk-pinning-progress').removeAttribute('value');
}

function onBulkPinningProgress(progress) {
  const isBulkPinningEnabled = $('bulk-pinning-toggle').checked;
  if (!isBulkPinningEnabled) {
    return;
  }
  $('bulk-pinning-setup-stage').innerText = progress.stage;
  $('bulk-pinning-available-disk-space').innerText =
      progress.availableDiskSpace;
  $('bulk-pinning-required-disk-space').innerText = progress.requiredDiskSpace;
  $('bulk-pinning-pinned-disk-space').innerText = progress.pinnedDiskSpace;
  $('bulk-pinning-progress').value = Number(progress.pinnedDiskSpace);
  $('bulk-pinning-progress').max = Number(progress.requiredDiskSpace);
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
  var freeSpaceInMB = toMegaByteString(localStorageSummary.free_space);
  $('local-storage-freespace').innerText = freeSpaceInMB;
}

/**
 * Updates the summary about in-flight operations.
 * @param {Array} inFlightOperations List of dictionaries describing the status
 * of in-flight operations.
 */
function updateInFlightOperations(inFlightOperations) {
  var container = $('in-flight-operations-contents');

  // Reset the table. Remove children in reverse order. Otherwides each
  // existingNodes[i] changes as a side effect of removeChild.
  var existingNodes = container.childNodes;
  for (var i = existingNodes.length - 1; i >= 0; i--) {
    var node = existingNodes[i];
    if (node.className == 'in-flight-operation') {
      container.removeChild(node);
    }
  }

  // Add in-flight operations.
  for (var i = 0; i < inFlightOperations.length; i++) {
    var operation = inFlightOperations[i];
    var tr = document.createElement('tr');
    tr.className = 'in-flight-operation';
    tr.appendChild(createElementFromText('td', operation.id));
    tr.appendChild(createElementFromText('td', operation.type));
    tr.appendChild(createElementFromText('td', operation.file_path));
    tr.appendChild(createElementFromText('td', operation.state));
    var progress = operation.progress_current + '/' + operation.progress_total;
    if (operation.progress_total > 0) {
      var percent = operation.progress_current / operation.progress_total * 100;
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
  var quotaTotalInMb = toMegaByteString(aboutResource['account-quota-total']);
  var quotaUsedInMb = toMegaByteString(aboutResource['account-quota-used']);

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
  var itemContainer = $('delta-update-status');
  for (var i = 0; i < deltaUpdateStatus['items'].length; i++) {
    var update = deltaUpdateStatus['items'][i];
    var tr = document.createElement('tr');
    tr.className = 'delta-update';
    tr.appendChild(createElementFromText('td', update.id));
    tr.appendChild(createElementFromText('td', update.root_entry_path));
    var startPageToken = update.start_page_token;
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
  var ul = $('event-log');
  updateKeyValueList(ul, log);
}

/**
 * Updates the service log section.
 * @param {Array} log Log lines.
 */
function updateServiceLog(log) {
  var ul = $('service-log');
  updateKeyValueList(ul, log);
}

/**
 * Updates the service log section.
 * @param {Array} log Log lines.
 */
function updateOtherServiceLogsUrl(url) {
  var link = $('other-logs');
  link.setAttribute('href', url);
}

/**
 * Adds a new row to the syncing paths table upon successful completion.
 * @param {string} path The path that was synced.
 * @param {string} status The drive::FileError as a string.
 */
function onAddSyncPath(path, status) {
  $('mirroring-path-status').textContent = status;
  if (status !== 'FILE_ERROR_OK') {
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
 * @param {string} status The drive::FileError as a string.
 */
function onRemoveSyncPath(path, status) {
  if (status !== 'FILE_ERROR_OK') {
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
  var element = document.createElement(elementName);
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
  for (var i = 0; i < list.length; i++) {
    var item = list[i];
    var text = item.key;
    if (item.value != '') {
      text += ': ' + item.value;
    }

    var li = createElementFromText('li', text);
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
  var toc = $('toc');
  while (toc.firstChild) {
    toc.removeChild(toc.firstChild);
  }
  var sections = document.getElementsByTagName('section');
  for (var i = 0; i < sections.length; i++) {
    var section = sections[i];
    if (!section.hidden) {
      var header = section.getElementsByTagName('h2')[0];
      var a = createElementFromText('a', header.textContent);
      a.href = '#' + section.id;
      var li = document.createElement('li');
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
  var element = $(section);
  if (element.hidden !== !enable) {
    element.hidden = !enable;
    updateToc();
  }
}

function onZipDone(success) {
  $('button-export-logs').removeAttribute('disabled');
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('pageLoaded');

  updateToc();

  $('verbose-logging-toggle').addEventListener('change', function(e) {
    chrome.send('setVerboseLoggingEnabled', [e.target.checked]);
  });

  $('mirroring-toggle').addEventListener('change', function(e) {
    chrome.send('setMirroringEnabled', [e.target.checked]);
  });

  $('bulk-pinning-toggle').addEventListener('change', function(e) {
    chrome.send('setBulkPinningEnabled', [e.target.checked]);
  });

  $('startup-arguments-form').addEventListener('submit', function(e) {
    e.preventDefault();
    $('arguments-status-text').textContent = 'applying...';
    chrome.send('setStartupArguments', [$('startup-arguments-input').value]);
  });

  $('mirror-path-form').addEventListener('submit', function(e) {
    e.preventDefault();
    $('mirroring-path-status').textContent = 'adding...';
    chrome.send('addSyncPath', [$('mirror-path-input').value]);
  });

  $('button-enable-tracing').addEventListener('click', function() {
    chrome.send('enableTracing');
  });

  $('button-disable-tracing').addEventListener('click', function() {
    chrome.send('disableTracing');
  });

  $('button-enable-networking').addEventListener('click', function() {
    chrome.send('enableNetworking');
  });

  $('button-disable-networking').addEventListener('click', function() {
    chrome.send('disableNetworking');
  });

  $('button-enable-force-pause-syncing').addEventListener('click', function() {
    chrome.send('enableForcePauseSyncing');
  });

  $('button-disable-force-pause-syncing').addEventListener('click', function() {
    chrome.send('disableForcePauseSyncing');
  });

  $('button-dump-account-settings').addEventListener('click', function() {
    chrome.send('dumpAccountSettings');
  });

  $('button-load-account-settings').addEventListener('click', function() {
    chrome.send('loadAccountSettings');
  });

  $('button-restart-drive').addEventListener('click', function() {
    chrome.send('restartDrive');
  });

  $('button-reset-drive-filesystem').addEventListener('click', function() {
    if (window.confirm(
            'Warning: Any local changes not yet uploaded to the Drive server ' +
            'will be lost, continue?')) {
      $('reset-status-text').textContent = 'resetting...';
      chrome.send('resetDriveFileSystem');
    }
  });

  $('button-export-logs').addEventListener('click', function() {
    $('button-export-logs').setAttribute('disabled', 'true');
    chrome.send('zipLogs');
  });

  window.setInterval(function() {
    chrome.send('periodicUpdate');
  }, 1000);
});
