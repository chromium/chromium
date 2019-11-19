// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The global load time data that contains the localized strings that we will
 * get from the main page when this page first loads.
 */
let loadTimeData = null;

/**
 * A queue of a sequence of closures that will incrementally build the sys info
 * html table.
 */
const tableCreationClosuresQueue = [];

/**
 * The time used to post delayed tasks in MS. Currently set to be enough for two
 * frames.
 */
const STANDARD_DELAY_MS = 32;

function getValueDivForButton(button) {
  return $(button.id.substr(0, button.id.length - 4));
}

function getButtonForValueDiv(valueDiv) {
  return $(valueDiv.id + '-btn');
}

/**
 * Expands the multiline table cell that contains the given valueDiv.
 * @param {HTMLElement} button The expand button.
 * @param {HTMLElement} valueDiv The div that contains the multiline logs.
 * @param {number} delayFactor A value used for increasing the delay after which
 *     the cell will be expanded. Useful for expandAll() since it expands the
 *     multiline cells one after another with each expension done slightly after
 *     the previous one.
 */
function expand(button, valueDiv, delayFactor) {
  button.textContent = loadTimeData.getString('sysinfoPageCollapseBtn');
  // Show the spinner container.
  const valueCell = valueDiv.parentNode;
  valueCell.removeAttribute('aria-hidden');
  valueCell.firstChild.hidden = false;
  // Expanding huge logs can take a very long time, so we do it after a delay
  // to have a chance to render the spinner.
  setTimeout(function() {
    valueCell.className = 'number-expanded';
    // Hide the spinner container.
    valueCell.firstChild.hidden = true;
  }, STANDARD_DELAY_MS * delayFactor);
}

/**
 * Collapses the multiline table cell that contains the given valueDiv.
 * @param {HTMLElement} button The expand button.
 * @param {HTMLElement} valueDiv The div that contains the multiline logs.
 */
function collapse(button, valueDiv) {
  button.textContent = loadTimeData.getString('sysinfoPageExpandBtn');
  valueDiv.parentNode.className = 'number-collapsed';
  // Don't have screen readers announce the empty cell.
  valueCell = valueDiv.parentNode;
  valueCell.setAttribute('aria-hidden', 'true');
}

/**
 * Toggles whether an item is collapsed or expanded.
 */
function changeCollapsedStatus() {
  const valueDiv = getValueDivForButton(this);
  if (valueDiv.parentNode.className == 'number-collapsed') {
    expand(this, valueDiv, 1);
  } else {
    collapse(this, valueDiv);
  }
}

/**
 * Collapses all log items.
 */
function collapseAll() {
  const valueDivs = document.getElementsByClassName('stat-value');
  for (let i = 0; i < valueDivs.length; ++i) {
    if (valueDivs[i].parentNode.className != 'number-expanded') {
      continue;
    }
    const button = getButtonForValueDiv(valueDivs[i]);
    if (button) {
      collapse(button, valueDivs[i]);
    }
  }
}

/**
 * Expands all log items.
 */
function expandAll() {
  const valueDivs = document.getElementsByClassName('stat-value');
  for (let i = 0; i < valueDivs.length; ++i) {
    if (valueDivs[i].parentNode.className != 'number-collapsed') {
      continue;
    }
    const button = getButtonForValueDiv(valueDivs[i]);
    if (button) {
      expand(button, valueDivs[i], i + 1);
    }
  }
}

function createNameCell(key) {
  const nameCell = document.createElement('td');
  nameCell.setAttribute('class', 'name');
  const nameDiv = document.createElement('div');
  nameDiv.setAttribute('class', 'stat-name');
  nameDiv.appendChild(document.createTextNode(key));
  nameCell.appendChild(nameDiv);
  return nameCell;
}

function createButtonCell(key, isMultiLine) {
  const buttonCell = document.createElement('td');
  buttonCell.setAttribute('class', 'button-cell');

  if (isMultiLine) {
    const button = document.createElement('button');
    button.setAttribute('id', '' + key + '-value-btn');
    button.setAttribute('aria-controls', '' + key + '-value');
    button.onclick = changeCollapsedStatus;
    button.textContent = loadTimeData.getString('sysinfoPageExpandBtn');
    buttonCell.appendChild(button);
  } else {
    // Don't have screen reader read the empty cell.
    buttonCell.setAttribute('aria-hidden', 'true');
  }

  return buttonCell;
}

function createValueCell(key, value, isMultiLine) {
  const valueCell = document.createElement('td');
  const valueDiv = document.createElement('div');
  valueDiv.setAttribute('class', 'stat-value');
  valueDiv.setAttribute('id', '' + key + '-value');
  valueDiv.appendChild(document.createTextNode(value));

  if (isMultiLine) {
    valueCell.className = 'number-collapsed';
    const loadingContainer = $('spinner-container').cloneNode(true);
    loadingContainer.setAttribute('id', '' + key + '-value-loading');
    loadingContainer.hidden = true;
    valueCell.appendChild(loadingContainer);
    // Don't have screen readers read the empty cell.
    valueCell.setAttribute('aria-hidden', 'true');
  } else {
    valueCell.className = 'number';
  }

  valueCell.appendChild(valueDiv);
  return valueCell;
}

function createTableRow(key, value) {
  const row = document.createElement('tr');

  // Avoid using element.scrollHeight as it's very slow. crbug.com/653968.
  const isMultiLine = value.split('\n').length > 2 || value.length > 1000;

  row.appendChild(createNameCell(key));
  row.appendChild(createButtonCell(key, isMultiLine));
  row.appendChild(createValueCell(key, value, isMultiLine));

  return row;
}

/**
 * Finalize the page after the content has been loaded.
 */
function finishPageLoading() {
  $('collapseAllBtn').onclick = collapseAll;
  $('expandAllBtn').onclick = expandAll;

  $('spinner-container').hidden = true;
}

/**
 * Pops a closure from the front of the queue and executes it.
 */
function processQueue() {
  const closure = tableCreationClosuresQueue.shift();
  if (closure) {
    closure();
  }

  if (tableCreationClosuresQueue.length > 0) {
    // Post a task to process the next item in the queue.
    setTimeout(processQueue, STANDARD_DELAY_MS);
  }
}

/**
 * Creates a closure that creates a table row for the given key and value.
 * @param {string} key The name of the log.
 * @param {string} value The contents of the log.
 * @return {function():void} A closure that creates a row for the given log.
 */
function createTableRowWrapper(key, value) {
  return function() {
    $('detailsTable').appendChild(createTableRow(key, value));
  };
}

/**
 * Creates closures to build the system information table row by row
 * incrementally.
 * @param {Object} systemInfo The system information that will be used to fill
 * the table.
 */
function createTable(systemInfo) {
  for (const key in systemInfo) {
    const item = systemInfo[key];
    tableCreationClosuresQueue.push(
        createTableRowWrapper(item['key'], item['value']));
  }

  tableCreationClosuresQueue.push(finishPageLoading);

  processQueue();
}

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  loadTimeData = getLoadTimeData();
  i18nTemplate.process(document, loadTimeData);
  getFullSystemInfo(createTable);
};
