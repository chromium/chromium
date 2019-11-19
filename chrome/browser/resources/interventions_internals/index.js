// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** The columns that are used to find rows that contain the keyword. */
const ENABLE_BLACKLIST_BUTTON = 'Enable Blacklist';
const IGNORE_BLACKLIST_BUTTON = 'Ignore Blacklist';
const IGNORE_BLACKLIST_MESSAGE = 'Blacklist decisions are ignored.';
const URL_THRESHOLD = 40;  // Maximum URL length

window.logTableMap = {};

/**
 * Helper method to pad number, used for time format.
 * @param {number} value The original number.
 * @param {number} length The desired number length.
 */
function getPaddedValue(value, length) {
  let result = '' + value;
  while (result.length < length) {
    result = '0' + result;
  }
  return result;
}

/**
 * Convert milliseconds to human readable date/time format.
 * The return format will be "MM/dd/YYYY hh:mm:ss.sss"
 * @param {number} time Time in millisecond since Unix Epoch.
 * @return The converted string format.
 */
function getTimeFormat(time) {
  const date = new Date(time);
  const options = {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
  };

  const dateString = date.toLocaleDateString('en-US', options);
  const hour = getPaddedValue(date.getHours(), 2);
  const min = getPaddedValue(date.getMinutes(), 2);
  const sec = getPaddedValue(date.getSeconds(), 2);
  const millisec = getPaddedValue(date.getMilliseconds(), 3);
  return dateString + ' ' + hour + ':' + min + ':' + sec + '.' + millisec;
}

/**
 * Append a button to |element|, so that when the button is clicked, the
 * detailed logs table associated with |pageId| will be shown/hidden.
 * @param {!HTMLElement} element The element that the button will be added to.
 * @param {number} pageId Used to locate the ID of the logs table row.
 */
function addMoreDetailsButton(element, pageId) {
  const moreDetailsButton = document.createElement('button');
  moreDetailsButton.setAttribute('class', 'more-details-button');
  element.appendChild(moreDetailsButton);

  const icon = document.createElement('i');
  icon.setAttribute('class', 'arrow down');
  moreDetailsButton.appendChild(icon);

  moreDetailsButton.addEventListener('click', () => {
    const expansionRow = $('expansion-row-' + pageId);
    expansionRow.className = (expansionRow.className.includes('hide')) ?
        expansionRow.className.replace('hide', 'show') :
        expansionRow.className.replace('show', 'hide');

    icon.className = (icon.className.includes('down')) ?
        icon.className.replace('down', 'up') :
        icon.className.replace('up', 'down');
  });
}

/**
 * Helper method to move a row to the top of a html table, below the header
 * row.
 * @param {!HTMLElement} row The row to move.
 * @param {!HTMLElement} table The table to move.
 */
function pushRowToTopOfLogsTable(row, table) {
  const newRow = table.insertRow(1);
  newRow.className = row.className;
  newRow.id = row.id;
  newRow.innerHTML = row.innerHTML;
  row.remove();
}

/**
 * Helper method to move a group of messages to the top of the Logs Table,
 * including the expansion row corresponding to the |pageId|.
 *
 * @param {number} pageId The key of |logTableMap| of the moving row.
 */
function pushMessagesToTopOfLogsTable(pageId) {
  const logsTable = $('message-logs-table');
  const currentMessageRow = window.logTableMap[pageId];

  // Moving empty row.
  const emptyRow = logsTable.rows[currentMessageRow.rowIndex + 2];
  pushRowToTopOfLogsTable(emptyRow, logsTable);

  // Moving expansion row.
  const expansionRow = logsTable.rows[currentMessageRow.rowIndex + 1];
  pushRowToTopOfLogsTable(expansionRow, logsTable);

  // Moving the original row.
  pushRowToTopOfLogsTable(currentMessageRow, logsTable);
  window.logTableMap[pageId] = logsTable.rows[1];
}

/**
 * Helper method to expand or collapse all logs in the message-logs-table.
 *
 * @param {boolean} expanding True for expand all log messages, and false to
 * collapse all log messages.
 */
function logExpansionHelper(expanding) {
  const rows = $('message-logs-table').rows;
  for (let i = 1; i < rows.length; i++) {
    if (rows[i].className.includes('expansion-row')) {
      rows[i].className = expanding ?
          rows[i].className.replace('hide', 'show') :
          rows[i].className.replace('show', 'hide');
      const arrowButton = rows[i - 1].querySelector('.arrow');
      if (arrowButton) {
        arrowButton.className = expanding ? 'arrow up' : 'arrow down';
      }
    }
  }
}

/**
 * Update the |pageId| log message group. Copy the main row that contains the
 * most updated log message of the group to the expansion row, and update the
 * current main row with new info.
 *
 * @param {number!} time Millisecond since Unix Epoch representation of time.
 * @param {string!} type The message event type.
 * @param {string!} description The event message description.
 * @param {string} url The URL associated with the event.
 */
function updateTableRowByPageId(time, type, description, url, pageId) {
  assert(pageId > 0);
  assert(window.logTableMap[pageId]);
  pushMessagesToTopOfLogsTable(pageId);

  const currentRow = window.logTableMap[pageId];
  const expansionRow = $('expansion-row-' + pageId);
  const newRow =
      expansionRow.querySelector('.expansion-logs-table').insertRow(0);
  newRow.setAttribute('class', 'expand-log-message');

  // Copying data from previous row, to the first row of the expansion table.
  currentRow.querySelectorAll('td').forEach((column) => {
    const cell = column.cloneNode(true);
    const expandButton = cell.querySelector('.more-details-button');
    if (expandButton) {
      expandButton.remove();
    }
    newRow.appendChild(cell);
  });

  // Update current row with new data.
  currentRow.querySelector('.log-time').textContent = getTimeFormat(time);
  currentRow.querySelector('.log-type').textContent = type;
  const descriptionTd = currentRow.querySelector('.log-description');
  descriptionTd.textContent = description;
  addMoreDetailsButton(descriptionTd, pageId);

  let urlTd = currentRow.querySelector('.log-url');
  if (urlTd) {
    urlTd.remove();
    if (url.length > 0) {
      urlTd = createUrlElement(url);
      urlTd.setAttribute('class', 'log-url');
      currentRow.appendChild(urlTd);
    }
  }
}

/**
 * Create an new row for expansion table below the |mainRow|.
 *
 * @param {!HTMLElement} mainRow The row with the most updated log event of the
 * group.
 * @param {number} pageId The ID associated with the group event.
 */
function createExpansionRow(mainRow, pageId) {
  const logsTable = $('message-logs-table');
  const expansionRow = logsTable.insertRow(mainRow.rowIndex + 1);
  expansionRow.setAttribute('class', 'expansion-row hide');
  expansionRow.setAttribute('id', 'expansion-row-' + pageId);
  window.logTableMap[pageId] = mainRow;

  const tdNode = document.createElement('td');
  tdNode.setAttribute('colspan', '4');
  expansionRow.appendChild(tdNode);

  const expansionTable = document.createElement('table');
  expansionTable.setAttribute('class', 'expansion-logs-table');
  tdNode.appendChild(expansionTable);

  // Insert row so that the table even/odd coloring remains the same.
  const hiddenRow = logsTable.insertRow(expansionRow.rowIndex + 1);
  hiddenRow.setAttribute('class', 'hide');
}

/**
 * Insert a log message row to the top of the log message table.
 *
 * @param {number!} time Millisecond since Unix Epoch representation of time.
 * @param {string!} type The message event type.
 * @param {string!} description The event message description.
 * @param {string} url The URL associated with the event.
 */
function insertMessageRowToMessageLogTable(
    time, type, description, url, pageId) {
  assert(pageId >= 0);
  if (pageId > 0 && window.logTableMap[pageId]) {
    updateTableRowByPageId(time, type, description, url, pageId);
    return;
  }

  const tableRow =
      $('message-logs-table').insertRow(1);  // Index 0 belongs to header row.
  tableRow.setAttribute('class', 'log-message');

  if (pageId > 0) {  // If the new message will be grouped.
    createExpansionRow(tableRow, pageId);
  }

  const timeTd = document.createElement('td');
  timeTd.textContent = getTimeFormat(time);
  timeTd.setAttribute('class', 'log-time');
  tableRow.appendChild(timeTd);

  const typeTd = document.createElement('td');
  typeTd.setAttribute('class', 'log-type');
  typeTd.textContent = type;
  tableRow.appendChild(typeTd);

  const descriptionTd = document.createElement('td');
  descriptionTd.setAttribute('class', 'log-description');
  descriptionTd.textContent = description;
  tableRow.appendChild(descriptionTd);

  if (url.length > 0) {
    const urlTd = createUrlElement(url);
    urlTd.setAttribute('class', 'log-url');
    tableRow.appendChild(urlTd);
  }
}

/**
 * Switch the selected tab to 'selected-tab' class.
 */
function setSelectedTab() {
  const selected =
      document.querySelector('input[type=radio][name=tabs]:checked');
  const selectedTab = document.querySelector('#' + selected.value);

  selectedTab.className =
      selectedTab.className.replace('hidden-tab', 'selected-tab');
  selected.parentElement.className =
      selected.parentElement.className.replace('inactive-tab', 'active-tab');
}

/**
 * Change the previously selected element to 'hidden-tab' class, and switch the
 * selected element to 'selected-tab' class.
 */
function changeTab() {
  const lastSelected = document.querySelector('.selected-tab');
  const lastTab = document.querySelector('.active-tab');
  lastSelected.className =
      lastSelected.className.replace('selected-tab', 'hidden-tab');
  lastTab.className = lastTab.className.replace('active-tab', 'inactive-tab');

  setSelectedTab();
}

/**
 * Helper function to check if all keywords, case insensitive, are in the given
 * text.
 *
 * @param {Array<string>} keywords The collection of keywords.
 * @param {string} text The given text to search.
 * @return True iff all keywords present in the given text.
 */
function checkTextContainsKeywords(keywords, text) {
  for (let i = 0; i < keywords.length; i++) {
    if (!text.toUpperCase().includes(keywords[i].toUpperCase())) {
      return false;
    }
  }
  return true;
}

/**
 * Initialize the navigation bar, and setup OnChange listeners for the tabs.
 */
function setupTabControl() {
  // Initialize on change listeners.
  const tabs = document.querySelectorAll('input[type=radio][name=tabs]');
  tabs.forEach((tab) => {
    tab.addEventListener('change', changeTab);
  });

  const tabContents = document.querySelectorAll('.tab-content');
  tabContents.forEach((tab) => {
    tab.className += ' hidden-tab';
  });

  // Turn on the default selected tab.
  setSelectedTab();
}

/**
 * Initialize the search functionality of the search bar on the log tab.
 * Searching will hide any rows that don't contain the keyword in the search
 * bar.
 */
function setupLogSearch() {
  $('log-search-bar').addEventListener('keyup', () => {
    const keys = $('log-search-bar').value.split(' ');
    const rows = $('message-logs-table').rows;
    logExpansionHelper(true /* expanding */);

    for (let i = 1; i < rows.length; i++) {
      // Check the main row.
      rows[i].style.display =
          checkTextContainsKeywords(keys, rows[i].textContent) ? '' : 'none';

      // Check expandable rows.
      const subtable = rows[i].querySelector('.expansion-logs-table');
      if (subtable) {
        for (let j = 0; j < subtable.rows.length; j++) {
          subtable.rows[j].style.display =
              checkTextContainsKeywords(keys, subtable.rows[j].textContent) ?
              '' :
              'none';
        }
      }
    }
  });
}

/**
 * Initialize the button to expand all logs data, and collapse all logs.
 */
function setupExpandLogs() {
  // Expand all button.
  $('expand-log-button').addEventListener('click', () => {
    logExpansionHelper(true /* expanding */);
    $('collapse-log-button').style.display = '';
    $('expand-log-button').style.display = 'none';
  });

  // Collapse all button.
  $('collapse-log-button').style.display = 'none';
  $('collapse-log-button').addEventListener('click', () => {
    logExpansionHelper(false /* expanding */);
    $('collapse-log-button').style.display = 'none';
    $('expand-log-button').style.display = '';
  });
}

/**
 * Create and add a copy to clipboard button to a given node.
 *
 * @param {string} text The text that will be copied to the clipboard.
 * @param {Element} node The node that will have the button appended to.
 */
function appendCopyToClipBoardButton(text, node) {
  if (!document.queryCommandSupported ||
      !document.queryCommandSupported('copy')) {
    // Don't add copy to clipboard button if not supported.
    return;
  }
  const copyButton = document.createElement('div');
  copyButton.setAttribute('class', 'copy-to-clipboard-button');
  copyButton.textContent = 'Copy';

  copyButton.addEventListener('click', () => {
    const textarea = document.createElement('textarea');
    textarea.textContent = text;
    document.body.appendChild(textarea);
    textarea.select();
    try {
      return document.execCommand('copy');  // Security exception may be thrown.
    } catch (ex) {
      console.warn('Copy to clipboard failed.', ex);
      return false;
    } finally {
      document.body.removeChild(textarea);
    }
  });
  node.appendChild(copyButton);
}

/**
 * Shorten long URL string so that it can be displayed nicely on mobile devices.
 * If |url| is longer than URL_THRESHOLD, then it will be shorten, and a tooltip
 * element will be added so that user can see the original URL.
 *
 * Add copy to clipboard button to it.
 *
 * @param {string} url The given URL string.
 * @return An DOM node with the original URL if the length is within THRESHOLD,
 * or the shorten URL with a tooltip element at the end of the string.
 */
function createUrlElement(url) {
  const urlCell = document.createElement('div');
  urlCell.setAttribute('class', 'log-url-value');
  const urlTd = document.createElement('td');
  urlTd.appendChild(urlCell);

  if (url.length <= URL_THRESHOLD) {
    urlCell.textContent = url;
  } else {
    urlCell.textContent = url.substring(0, URL_THRESHOLD - 3) + '...';
    const tooltip = document.createElement('span');
    tooltip.setAttribute('class', 'url-tooltip');
    tooltip.textContent = url;
    urlTd.appendChild(tooltip);
  }

  // Append copy to clipboard button.
  appendCopyToClipBoardButton(url, urlTd);
  return urlTd;
}

/**
 * Helper function to remove all log message from log-messages-table.
 */
function removeAllLogMessagesRows() {
  const logsTable = $('message-logs-table');
  for (let row = logsTable.rows.length - 1; row > 0; row--) {
    logsTable.deleteRow(row);
  }
}

/**
 * Initialize the button to clear out all the log messages. This button only
 * remove the logs from the UI, and does not effect any decision made.
 */
function setupLogClear() {
  $('clear-log-button').addEventListener('click', removeAllLogMessagesRows);
}

/**
 * @constructor
 * @implements {mojom.InterventionsInternalsPageInterface}
 */
const InterventionsInternalPageImpl = function() {
  this.receiver_ = new mojom.InterventionsInternalsPageReceiver(this);
};

InterventionsInternalPageImpl.prototype = {
  /**
   * Post a new log message to the web page.
   *
   * @override
   * @param {!mojom.MessageLog} log The new log message recorded by
   * PreviewsLogger.
   */
  logNewMessage: function(log) {
    insertMessageRowToMessageLogTable(
        log.time, log.type, log.description, log.url.url, log.pageId);
  },

  /**
   * Update new blacklisted host to the web page.
   *
   * @override
   * @param {!string} host The blacklisted host.
   * @param {number} time The time when the host was blacklisted in milliseconds
   * since Unix epoch.
   */
  onBlacklistedHost: function(host, time) {
    const row = document.createElement('tr');
    row.setAttribute('class', 'blacklisted-host-row');

    const hostTd = document.createElement('td');
    hostTd.setAttribute('class', 'host-blacklisted');
    hostTd.textContent = host;
    row.appendChild(hostTd);

    const timeTd = document.createElement('td');
    timeTd.setAttribute('class', 'host-blacklisted-time');
    timeTd.textContent = getTimeFormat(time);
    row.appendChild(timeTd);

    // TODO(thanhdle): Insert row at correct index. crbug.com/776105.
    $('blacklisted-hosts-table').appendChild(row);
  },

  /**
   * Update to the page that the user blacklisted status has changed.
   *
   * @override
   * @param {boolean} blacklisted The time of the event in milliseconds since
   * Unix epoch.
   */
  onUserBlacklistedStatusChange: function(blacklisted) {
    const userBlacklistedStatus = $('user-blacklisted-status-value');
    userBlacklistedStatus.textContent =
        (blacklisted ? 'Blacklisted' : 'Not blacklisted');
  },

  /**
   * Update the blacklist cleared status on the page.
   *
   * @override
   * @param {number} time The time of the event in milliseconds since Unix
   * epoch.
   */
  onBlacklistCleared: function(time) {
    const blacklistClearedStatus = $('blacklist-last-cleared-time');
    blacklistClearedStatus.textContent = getTimeFormat(time);

    // Remove hosts from table.
    const blacklistedHostsTable = $('blacklisted-hosts-table');
    for (let row = blacklistedHostsTable.rows.length - 1; row > 0; row--) {
      blacklistedHostsTable.deleteRow(row);
    }

    // Remove log message from logs table.
    removeAllLogMessagesRows();

    // Log event message.
    insertMessageRowToMessageLogTable(
        time, 'Blacklist', 'Blacklist Cleared', '' /* URL */, 0 /* pageId */);
  },

  /**
   * Update the page with the new value of ignored blacklist decision status.
   *
   * @override
   * @param {boolean} ignored The new status of whether the previews blacklist
   * decisions is blacklisted or not.
   */
  onIgnoreBlacklistDecisionStatusChanged: function(ignored) {
    const ignoreButton = $('ignore-blacklist-button');
    ignoreButton.textContent =
        ignored ? ENABLE_BLACKLIST_BUTTON : IGNORE_BLACKLIST_BUTTON;

    // Update the status of blacklist ignored on the page.
    $('blacklist-ignored-status').textContent =
        ignored ? IGNORE_BLACKLIST_MESSAGE : '';
  },

  /**
   * Update the page with the new value of estimated Effective Connection Type
   * (ECT). Log the ECT to the ECT logs table.
   *
   * @override
   * @param {string} type The string representation of estimated ECT.
   * @param {string} maxInterventionType The string representation of the
   * session's maximum ECT threshold for interventions.
   */
  updateEffectiveConnectionType: function(type, maxInterventionType) {
    // Change the current ECT.
    const ectType = $('nqe-type');
    ectType.textContent = type;

    // Set the session maximum ECT for interventions.
    const maxInterventionEctType = $('max-intervention-type');
    maxInterventionEctType.textContent = maxInterventionType;

    const now = getTimeFormat(Date.now());

    // Log ECT changed event to ECT change log.
    const nqeRow =
        $('nqe-logs-table').insertRow(1);  // Index 0 belongs to header row.

    const timeCol = document.createElement('td');
    timeCol.textContent = now;
    timeCol.setAttribute('class', 'nqe-time-column');
    nqeRow.appendChild((timeCol));

    const nqeCol = document.createElement('td');
    nqeCol.setAttribute('class', 'nqe-value-column');
    nqeCol.textContent = type;
    nqeRow.appendChild(nqeCol);
  },

  /**
   * Returns a remote interface to the receiver.
   */
  bindNewPipeAndPassRemote: function() {
    const helper = this.receiver_.$;
    return helper.bindNewPipeAndPassRemote();
  },
};

cr.define('interventions_internals', () => {
  let pageHandler = null;

  function init(handler) {
    pageHandler = handler;
    getPreviewsEnabled();
    getPreviewsFlagsDetails();

    const ignoreButton = $('ignore-blacklist-button');
    ignoreButton.addEventListener('click', () => {
      // Whether the blacklist is currently ignored.
      const ignored = (ignoreButton.textContent == ENABLE_BLACKLIST_BUTTON);
      // Try to reverse the ignore status.
      pageHandler.setIgnorePreviewsBlacklistDecision(!ignored);
    });
  }

  /**
   * Retrieves the statuses of previews (i.e. Offline, Lite Pages, etc),
   * and posts them on chrome://intervention-internals.
   */
  function getPreviewsEnabled() {
    pageHandler.getPreviewsEnabled()
        .then((response) => {
          const statuses = $('previews-enabled-status');

          response.statuses.forEach((value) => {
            let message = value.description + ': ';
            const key = value.htmlId;
            message += value.enabled ? 'Enabled' : 'Disabled';

            assert(!$(key), 'Component ' + key + ' already existed!');

            const node = document.createElement('div');
            node.setAttribute('class', 'previews-status-value');
            node.setAttribute('id', key);
            node.textContent = message;
            statuses.appendChild(node);
          });
        })
        .catch((error) => {
          console.error(error.message);
        });
  }

  function getPreviewsFlagsDetails() {
    pageHandler.getPreviewsFlagsDetails()
        .then((response) => {
          const flags = $('previews-flags-table');

          response.flags.forEach((flag) => {
            const key = flag.htmlId;
            assert(!$(key), 'Component ' + key + ' already existed!');

            const flagDescription = document.createElement('a');
            flagDescription.setAttribute('class', 'previews-flag-description');
            flagDescription.setAttribute('id', key + 'Description');
            flagDescription.setAttribute('href', flag.link);
            flagDescription.textContent = flag.description;

            const flagNameTd = document.createElement('td');
            flagNameTd.appendChild(flagDescription);

            const flagValueTd = document.createElement('td');
            flagValueTd.setAttribute('class', 'previews-flag-value');
            flagValueTd.setAttribute('id', key + 'Value');
            flagValueTd.textContent = flag.value;

            const node = document.createElement('tr');
            node.setAttribute('class', 'previews-flag-container');
            node.appendChild(flagNameTd);
            node.appendChild(flagValueTd);
            flags.appendChild(node);
          });
        })
        .catch((error) => {
          console.error(error.message);
        });
  }

  return {
    init: init,
  };
});

window.setupFn = window.setupFn || function() {
  return Promise.resolve();
};

document.addEventListener('DOMContentLoaded', () => {
  setupTabControl();
  setupLogSearch();
  setupLogClear();
  setupExpandLogs();
  let pageHandler = null;
  let pageImpl = null;

  window.setupFn().then(() => {
    if (window.testPageHandler) {
      pageHandler = window.testPageHandler;
    } else {
      pageHandler = mojom.InterventionsInternalsPageHandler.getRemote();

      // Set up client side mojo interface.
      pageImpl = new InterventionsInternalPageImpl();
      pageHandler.setClientPage(pageImpl.bindNewPipeAndPassRemote());
    }

    interventions_internals.init(pageHandler);
  });
});
