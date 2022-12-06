// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {$} from 'chrome://resources/js/util_ts.js';

// Contents of lines that act as delimiters for multi-line values.
const DELIM_START = '---------- START ----------';
const DELIM_END = '---------- END ----------';

// Limit file size to 10 MiB to prevent hanging on accidental upload.
const MAX_FILE_SIZE = 10485760;

// <if expr="chromeos_ash">
// Link to markdown doc with documentation for Chrome OS.
const CROS_MD_DOC_URL =
    'https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/debugd/docs/log_entries.md';
// </if>

function getValueDivForButton(button) {
  return $(button.id.substr(0, button.id.length - 4));
}

function getButtonForValueDiv(valueDiv) {
  return $(valueDiv.id + '-btn');
}

function handleDragOver(e) {
  e.dataTransfer.dropEffect = 'copy';
  e.preventDefault();
}

function handleDrop(e) {
  const file = e.dataTransfer.files[0];
  if (file) {
    e.preventDefault();
    importLog(file);
  }
}

function showError(fileName) {
  $('status').textContent = loadTimeData.getStringF('parseError', fileName);
}

/**
 * Toggles whether an item is collapsed or expanded.
 */
function changeCollapsedStatus() {
  const valueDiv = getValueDivForButton(this);
  if (valueDiv.parentNode.className === 'number-collapsed') {
    valueDiv.parentNode.className = 'number-expanded';
    this.textContent = loadTimeData.getString('collapseBtn');
  } else {
    valueDiv.parentNode.className = 'number-collapsed';
    this.textContent = loadTimeData.getString('expandBtn');
  }
}

/**
 * Collapses all log items.
 */
function collapseAll() {
  const valueDivs = document.getElementsByClassName('stat-value');
  for (let i = 0; i < valueDivs.length; i++) {
    const button = getButtonForValueDiv(valueDivs[i]);
    if (button && button.className !== 'button-hidden') {
      button.textContent = loadTimeData.getString('expandBtn');
      valueDivs[i].parentNode.className = 'number-collapsed';
    }
  }
}

/**
 * Expands all log items.
 */
function expandAll() {
  const valueDivs = document.getElementsByClassName('stat-value');
  for (let i = 0; i < valueDivs.length; i++) {
    const button = getButtonForValueDiv(valueDivs[i]);
    if (button && button.className !== 'button-hidden') {
      button.textContent = loadTimeData.getString('collapseBtn');
      valueDivs[i].parentNode.className = 'number-expanded';
    }
  }
}

/**
 * Read in a log asynchronously, calling parseSystemLog if successful.
 * @param {File} file The file to read.
 */
function importLog(file) {
  if (file && file.size <= MAX_FILE_SIZE) {
    const reader = new FileReader();
    reader.onload = function() {
      if (parseSystemLog(this.result)) {
        // Reset table title and status
        $('tableTitle').textContent =
            loadTimeData.getStringF('logFileTableTitle', file.name);
        $('status').textContent = '';
      } else {
        showError(file.name);
      }
    };
    reader.readAsText(file);
  } else if (file) {
    showError(file.name);
  }
}

/**
 * For a particular log entry, create the DOM node representing it in the
 * log entry table.
 * @param{log} A dictionary with the keys statName and statValue
 * @return{Element} The DOM node for the given log entry.
 */
function createNodeForLogEntry(log) {
  const row = document.createElement('tr');

  const nameCell = document.createElement('td');
  nameCell.className = 'name';
  const nameDiv = document.createElement('div');
  nameDiv.className = 'stat-name';

  // Add an anchor link that links to the log entry.
  const anchor = document.createElement('a');
  anchor.href = `#${log.statName}`;
  anchor.className = 'anchor';
  nameDiv.appendChild(anchor);

  const a = document.createElement('a');
  a.className = 'stat-name-link';

  // Let URL be anchor to the section of this page by default.
  let urlPrefix = '';
  // <if expr="chromeos_ash">
  // Link to the markdown doc with documentation for the entry for Chrome OS
  // instead.
  urlPrefix = CROS_MD_DOC_URL;
  // </if>
  a.href = `${urlPrefix}#${log.statName}`;
  a.name = a.text = log.statName;
  nameDiv.appendChild(a);
  nameCell.appendChild(nameDiv);
  row.appendChild(nameCell);

  const buttonCell = document.createElement('td');
  buttonCell.className = 'button-cell';
  const button = document.createElement('button');
  button.id = log.statName + '-value-btn';
  button.className = 'expand-status';
  button.onclick = changeCollapsedStatus;
  buttonCell.appendChild(button);
  row.appendChild(buttonCell);

  const valueCell = document.createElement('td');
  const valueDiv = document.createElement('div');
  valueDiv.className = 'stat-value';
  valueDiv.id = log.statName + '-value';
  valueDiv.textContent = log.statValue;
  valueCell.appendChild(valueDiv);
  row.appendChild(valueCell);

  if (log.statValue.length > 200) {
    button.className = '';
    button.textContent = loadTimeData.getString('expandBtn');
    valueCell.className = 'number-collapsed';
  } else {
    button.className = 'button-hidden';
    valueCell.className = 'number';
  }

  return row;
}

/**
 * Given a list of log entries, replace the contents of the log entry table
 * with those entries. The log entries are passed as a list of dictionaries
 * containing the keys statName and statValue.
 * @param {systemInfo} The log entries to insert into the DOM.
 */
function updateLogEntries(systemInfo) {
  const fragment = document.createDocumentFragment();
  systemInfo.forEach(logEntry => {
    const node = createNodeForLogEntry(logEntry);
    fragment.appendChild(node);
  });
  const table = $('details');

  // Delete any existing log entries in the table
  table.innerHTML = trustedTypes.emptyHTML;
  table.appendChild(fragment);
}

/**
 * Callback called when system info has been fetched. The log entries are passed
 * as a list of dictionaries containing the keys statName and statValue.
 * @param {systemInfo} The fetched log entries.
 */
function returnSystemInfo(systemInfo) {
  updateLogEntries(systemInfo);
  const spinner = $('loadingIndicator');
  spinner.style.display = 'none';
  spinner.style.animationPlayState = 'paused';
}

/**
 * Convert text-based log into list of name-value pairs.
 * @param {string} text The raw text of a log.
 * @return {boolean} True if the log was parsed successfully.
 */
function parseSystemLog(text) {
  const details = [];
  const lines = text.split('\n');
  for (let i = 0, len = lines.length; i < len; i++) {
    // Skip empty lines.
    if (!lines[i]) {
      continue;
    }

    const delimiter = lines[i].indexOf('=');
    if (delimiter <= 0) {
      if (i === lines.length - 1) {
        break;
      }
      // If '=' is missing here, format is wrong.
      return false;
    }

    const name = lines[i].substring(0, delimiter);
    let value = '';
    // Set value if non-empty
    if (lines[i].length > delimiter + 1) {
      value = lines[i].substring(delimiter + 1);
    }

    // Delimiters are based on kMultilineIndicatorString, kMultilineStartString,
    // and kMultilineEndString in components/feedback/feedback_data.cc.
    // If these change, we should check for both the old and new versions.
    if (value === '<multiline>') {
      // Skip start delimiter.
      if (i === len - 1 || lines[++i].indexOf(DELIM_START) === -1) {
        return false;
      }

      ++i;
      value = '';
      // Append lines between start and end delimiters.
      while (i < len && lines[i] !== DELIM_END) {
        value += lines[i++] + '\n';
      }

      // Remove trailing newline.
      if (value) {
        value = value.substr(0, value.length - 1);
      }
    }
    details.push({'statName': name, 'statValue': value});
  }



  updateLogEntries(details);

  return true;
}

document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('requestSystemInfo').then(returnSystemInfo);

  $('collapseAll').onclick = collapseAll;
  $('expandAll').onclick = expandAll;

  const tp = $('t');
  tp.addEventListener('dragover', handleDragOver, false);
  tp.addEventListener('drop', handleDrop, false);
});
