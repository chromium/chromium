// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

/**
 * A queue of a sequence of closures that will incrementally build the sys info
 * html table.
 */
const tableCreationClosuresQueue: Array<() => void> = [];

/**
 * The time used to post delayed tasks in MS. Currently set to be enough for two
 * frames.
 */
const STANDARD_DELAY_MS: number = 32;

/**
 * The total count of rows that have an Expand/Collapse button. This is needed
 * to calculate the aria-pressed state of the global Expand All/Collapse All
 * buttons.
 */
let multilineRowsCount = 0;

/**
 * Running count of rows that have been expanded to display all lines. This is
 * needed to calculate the aria-pressed state of the global Expand All/Collapse
 * All buttons.
 */
let expandedRowsCount = 0;

function updateGlobalExpandButtonStates() {
  const hasExpanded = expandedRowsCount > 0;
  const hasCollapsed = multilineRowsCount - expandedRowsCount > 0;

  if (hasExpanded && hasCollapsed) {
    getRequiredElement('expandAllBtn').ariaPressed = 'mixed';
    getRequiredElement('collapseAllBtn').ariaPressed = 'mixed';
  } else if (hasExpanded && !hasCollapsed) {
    getRequiredElement('expandAllBtn').ariaPressed = 'true';
    getRequiredElement('collapseAllBtn').ariaPressed = 'false';
  } else if (!hasExpanded && hasCollapsed) {
    getRequiredElement('expandAllBtn').ariaPressed = 'false';
    getRequiredElement('collapseAllBtn').ariaPressed = 'true';
  } else {
    getRequiredElement('expandAllBtn').ariaPressed = 'false';
    getRequiredElement('collapseAllBtn').ariaPressed = 'false';
  }
}

function getValueDivForButton(button: HTMLElement) {
  return getRequiredElement(button.id.substr(0, button.id.length - 4));
}

function getButtonForValueDiv(valueDiv: HTMLElement) {
  return getRequiredElement(valueDiv.id + '-btn');
}

/**
 * Expands the multiline table cell that contains the given valueDiv.
 * @param button The expand button.
 * @param valueDiv The div that contains the multiline logs.
 * @param delayFactor A value used for increasing the delay after which the cell
 *     will be expanded. Useful for expandAll() since it expands the multiline
 *     cells one after another with each expension done slightly after the
 *     previous one.
 */
function expand(
    button: HTMLElement, valueDiv: HTMLElement, delayFactor: number) {
  button.textContent = loadTimeData.getString('logsMapPageCollapseBtn');
  // Show the spinner container.
  const valueCell = valueDiv.parentNode as HTMLElement;
  valueCell.removeAttribute('aria-hidden');
  (valueCell.firstChild as HTMLElement).hidden = false;
  // Expanding huge logs can take a very long time, so we do it after a delay
  // to have a chance to render the spinner.
  setTimeout(function() {
    valueCell.className = 'number-expanded';
    // Hide the spinner container.
    (valueCell.firstChild as HTMLElement).hidden = true;
  }, STANDARD_DELAY_MS * delayFactor);
  expandedRowsCount++;
}

/**
 * Collapses the multiline table cell that contains the given valueDiv.
 * @param button The expand button.
 * @param valueDiv The div that contains the multiline logs.
 */
function collapse(button: HTMLElement, valueDiv: HTMLElement) {
  button.textContent = loadTimeData.getString('logsMapPageExpandBtn');
  (valueDiv.parentNode as HTMLElement).className = 'number-collapsed';
  // Don't have screen readers announce the empty cell.
  const valueCell = valueDiv.parentNode as HTMLElement;
  valueCell.setAttribute('aria-hidden', 'true');
  expandedRowsCount--;
}

/**
 * Toggles whether an item is collapsed or expanded.
 */
function changeCollapsedStatus(e: Event) {
  const button = e.target as HTMLElement;
  const valueDiv = getValueDivForButton(button);
  if ((valueDiv.parentNode as HTMLElement).className === 'number-collapsed') {
    expand(button, valueDiv, 1);
  } else {
    collapse(button, valueDiv);
  }

  updateGlobalExpandButtonStates();
}

/**
 * Collapses all log items.
 */
function collapseAll() {
  const valueDivs = document.body.querySelectorAll<HTMLElement>('.stat-value');
  for (let i = 0; i < valueDivs.length; ++i) {
    if ((valueDivs[i]!.parentNode as HTMLElement).className !==
        'number-expanded') {
      continue;
    }
    const button = getButtonForValueDiv(valueDivs[i]!);
    if (button) {
      collapse(button, valueDivs[i]!);
    }
  }

  updateGlobalExpandButtonStates();
}

/**
 * Expands all log items.
 */
function expandAll() {
  const valueDivs = document.body.querySelectorAll<HTMLElement>('.stat-value');
  for (let i = 0; i < valueDivs.length; ++i) {
    if ((valueDivs[i]!.parentNode as HTMLElement).className !==
        'number-collapsed') {
      continue;
    }
    const button = getButtonForValueDiv(valueDivs[i]!);
    if (button) {
      expand(button, valueDivs[i]!, i + 1);
    }
  }

  updateGlobalExpandButtonStates();
}

function createNameCell(key: string): HTMLElement {
  const nameCell = document.createElement('td');
  nameCell.setAttribute('class', 'name');
  const nameDiv = document.createElement('div');
  nameDiv.id = key;
  nameDiv.setAttribute('class', 'stat-name');
  nameDiv.appendChild(document.createTextNode(key));
  nameCell.appendChild(nameDiv);
  return nameCell;
}

function createButtonCell(key: string, isMultiLine: boolean): HTMLElement {
  const buttonCell = document.createElement('td');
  buttonCell.setAttribute('class', 'button-cell');

  if (isMultiLine) {
    const id = `${key}-value-btn`;
    const button = document.createElement('button');
    button.setAttribute('id', id);
    button.setAttribute('aria-controls', '' + key + '-value');
    button.setAttribute('aria-labelledby', `${id} ${key}`);
    button.onclick = changeCollapsedStatus;
    button.textContent = loadTimeData.getString('logsMapPageExpandBtn');
    buttonCell.appendChild(button);
    multilineRowsCount++;
  } else {
    // Don't have screen reader read the empty cell.
    buttonCell.setAttribute('aria-hidden', 'true');
  }

  return buttonCell;
}

function createValueCell(
    key: string, value: string, isMultiLine: boolean): HTMLElement {
  const valueCell = document.createElement('td');
  const valueDiv = document.createElement('div');
  valueDiv.setAttribute('class', 'stat-value');
  valueDiv.setAttribute('id', '' + key + '-value');
  valueDiv.appendChild(document.createTextNode(value));

  if (isMultiLine) {
    valueCell.className = 'number-collapsed';
    const loadingContainer =
        getRequiredElement('spinner-container').cloneNode(true) as HTMLElement;
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

function createTableRow(key: string, value: string): HTMLElement {
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
  getRequiredElement('collapseAllBtn').onclick = collapseAll;
  getRequiredElement('expandAllBtn').onclick = expandAll;

  getRequiredElement('spinner-container').hidden = true;
  updateGlobalExpandButtonStates();
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
 * @param key The name of the log.
 * @param value The contents of the log.
 * @return A closure that creates a row for the given log.
 */
function createTableRowWrapper(key: string, value: string): () => void {
  return function() {
    getRequiredElement('detailsTable').appendChild(createTableRow(key, value));
  };
}

/**
 * Creates closures to build the logs table row by row incrementally.
 * @param info The information that will be used to fill the table.
 */
export function createLogsMapTable(
    info: chrome.feedbackPrivate.LogsMapEntry[]) {
  for (const key in info) {
    const item = info[key]!;
    tableCreationClosuresQueue.push(
        createTableRowWrapper(item['key'], item['value']));
  }

  tableCreationClosuresQueue.push(finishPageLoading);

  processQueue();
}
