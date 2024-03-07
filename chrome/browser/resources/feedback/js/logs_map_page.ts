// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

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

class LogsMapTable {
  /**
   * The total count of rows that have an Expand/Collapse button. This is needed
   * to calculate the aria-pressed state of the global Expand All/Collapse All
   * buttons.
   */
  private multilineRowsCount: number = 0;

  /**
   * Running count of rows that have been expanded to display all lines. This is
   * needed to calculate the aria-pressed state of the global Expand
   * All/Collapse All buttons.
   */
  private expandedRowsCount: number = 0;

  private root: ShadowRoot;

  constructor(root: ShadowRoot) {
    this.root = root;
  }

  private updateGlobalExpandButtonStates() {
    const hasExpanded = this.expandedRowsCount > 0;
    const hasCollapsed = this.multilineRowsCount - this.expandedRowsCount > 0;

    if (hasExpanded && hasCollapsed) {
      this.getRequiredElement('#expandAllBtn').ariaPressed = 'mixed';
      this.getRequiredElement('#collapseAllBtn').ariaPressed = 'mixed';
    } else if (hasExpanded && !hasCollapsed) {
      this.getRequiredElement('#expandAllBtn').ariaPressed = 'true';
      this.getRequiredElement('#collapseAllBtn').ariaPressed = 'false';
    } else if (!hasExpanded && hasCollapsed) {
      this.getRequiredElement('#expandAllBtn').ariaPressed = 'false';
      this.getRequiredElement('#collapseAllBtn').ariaPressed = 'true';
    } else {
      this.getRequiredElement('#expandAllBtn').ariaPressed = 'false';
      this.getRequiredElement('#collapseAllBtn').ariaPressed = 'false';
    }
  }

  private getValueDivForButton(button: HTMLElement): HTMLElement {
    const id = button.id.substr(0, button.id.length - 4);
    return this.getRequiredElement<HTMLElement>(`#${id}`)!;
  }

  private getButtonForValueDiv(valueDiv: HTMLElement): HTMLElement {
    const id = valueDiv.id + '-btn';
    return this.getRequiredElement<HTMLElement>(`#${id}`)!;
  }

  /**
   * Expands the multiline table cell that contains the given valueDiv.
   * @param button The expand button.
   * @param valueDiv The div that contains the multiline logs.
   * @param delayFactor A value used for increasing the delay after which the
   *     cell will be expanded. Useful for expandAll() since it expands the
   *     multiline cells one after another with each expension done slightly
   *     after the previous one.
   */
  private expand(
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
    this.expandedRowsCount++;
  }

  /**
   * Collapses the multiline table cell that contains the given valueDiv.
   * @param button The expand button.
   * @param valueDiv The div that contains the multiline logs.
   */
  private collapse(button: HTMLElement, valueDiv: HTMLElement) {
    button.textContent = loadTimeData.getString('logsMapPageExpandBtn');
    (valueDiv.parentNode as HTMLElement).className = 'number-collapsed';
    // Don't have screen readers announce the empty cell.
    const valueCell = valueDiv.parentNode as HTMLElement;
    valueCell.setAttribute('aria-hidden', 'true');
    this.expandedRowsCount--;
  }

  /**
   * Toggles whether an item is collapsed or expanded.
   */
  private changeCollapsedStatus(e: Event) {
    const button = e.target as HTMLElement;
    const valueDiv = this.getValueDivForButton(button);
    if ((valueDiv.parentNode as HTMLElement).className === 'number-collapsed') {
      this.expand(button, valueDiv, 1);
    } else {
      this.collapse(button, valueDiv);
    }

    this.updateGlobalExpandButtonStates();
  }

  /**
   * Collapses all log items.
   */
  private collapseAll() {
    const valueDivs = this.root.querySelectorAll<HTMLElement>('.stat-value');
    for (let i = 0; i < valueDivs.length; ++i) {
      if ((valueDivs[i]!.parentNode as HTMLElement).className !==
          'number-expanded') {
        continue;
      }
      const button = this.getButtonForValueDiv(valueDivs[i]!);
      if (button) {
        this.collapse(button, valueDivs[i]!);
      }
    }

    this.updateGlobalExpandButtonStates();
  }

  /**
   * Expands all log items.
   */
  private expandAll() {
    const valueDivs = this.root.querySelectorAll<HTMLElement>('.stat-value');
    for (let i = 0; i < valueDivs.length; ++i) {
      if ((valueDivs[i]!.parentNode as HTMLElement).className !==
          'number-collapsed') {
        continue;
      }
      const button = this.getButtonForValueDiv(valueDivs[i]!);
      if (button) {
        this.expand(button, valueDivs[i]!, i + 1);
      }
    }

    this.updateGlobalExpandButtonStates();
  }

  private sanitizeKeyForId(key: string): string {
    // Replace any non-alphanumeric characters with a dash.
    key = key.replace(/[^a-zA-Z0-9\-_]/g, '-');

    // Ensure the ID starts with a letter
    if (!/^[a-zA-Z]/.test(key)) {
      key = 'sanitized-' + key;
    }
    return key;
  }

  private createNameCell(key: string): HTMLElement {
    const nameCell = document.createElement('td');
    nameCell.setAttribute('class', 'name');
    const nameDiv = document.createElement('div');
    nameDiv.id = this.sanitizeKeyForId(key);
    nameDiv.setAttribute('class', 'stat-name');
    nameDiv.appendChild(document.createTextNode(key));
    nameCell.appendChild(nameDiv);
    return nameCell;
  }

  private createButtonCell(key: string, isMultiLine: boolean): HTMLElement {
    const buttonCell = document.createElement('td');
    buttonCell.setAttribute('class', 'button-cell');

    if (isMultiLine) {
      const sanitizedKey = this.sanitizeKeyForId(key);
      const id = `${sanitizedKey}-value-btn`;
      const button = document.createElement('button');
      button.setAttribute('id', id);
      button.setAttribute('aria-controls', `${sanitizedKey}-value`);
      button.setAttribute('aria-labelledby', `${id} ${sanitizedKey}`);
      button.onclick = this.changeCollapsedStatus.bind(this);
      button.textContent = loadTimeData.getString('logsMapPageExpandBtn');
      buttonCell.appendChild(button);
      this.multilineRowsCount++;
    } else {
      // Don't have screen reader read the empty cell.
      buttonCell.setAttribute('aria-hidden', 'true');
    }

    return buttonCell;
  }

  private createValueCell(key: string, value: string, isMultiLine: boolean):
      HTMLElement {
    const valueCell = document.createElement('td');
    const valueDiv = document.createElement('div');
    valueDiv.setAttribute('class', 'stat-value');
    valueDiv.setAttribute('id', this.sanitizeKeyForId(key) + '-value');
    valueDiv.appendChild(document.createTextNode(value));

    if (isMultiLine) {
      valueCell.className = 'number-collapsed';
      const loadingContainer =
          this.getRequiredElement('#spinner-container').cloneNode(true) as
          HTMLElement;
      loadingContainer.setAttribute(
          'id', this.sanitizeKeyForId(key) + '-value-loading');
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

  private createTableRow(key: string, value: string): HTMLElement {
    const row = document.createElement('tr');

    // Avoid using element.scrollHeight as it's very slow. crbug.com/653968.
    const isMultiLine = value.split('\n').length > 2 || value.length > 1000;

    row.appendChild(this.createNameCell(key));
    row.appendChild(this.createButtonCell(key, isMultiLine));
    row.appendChild(this.createValueCell(key, value, isMultiLine));

    return row;
  }

  /**
   * Finalize the page after the content has been loaded.
   */
  finishPageLoading() {
    this.getRequiredElement<HTMLElement>('#collapseAllBtn')!.onclick =
        this.collapseAll.bind(this);
    this.getRequiredElement<HTMLElement>('#expandAllBtn')!.onclick =
        this.expandAll.bind(this);

    this.getRequiredElement<HTMLElement>('#spinner-container')!.hidden = true;
    this.updateGlobalExpandButtonStates();
  }

  /**
   * Pops a closure from the front of the queue and executes it.
   */
  processQueue() {
    const closure = tableCreationClosuresQueue.shift();
    if (closure) {
      closure();
    }

    if (tableCreationClosuresQueue.length > 0) {
      // Post a task to process the next item in the queue.
      setTimeout(this.processQueue.bind(this), STANDARD_DELAY_MS);
    }
  }

  /**
   * Creates a closure that creates a table row for the given key and value.
   * @param key The name of the log.
   * @param value The contents of the log.
   * @return A closure that creates a row for the given log.
   */
  createTableRowWrapper(key: string, value: string): () => void {
    return () => {
      this.getRequiredElement('#detailsTable')
          .appendChild(this.createTableRow(key, value));
    };
  }

  /**
   * TODO(crbug.com/1509032): A helper function in favor of converting feedback
   * UI from non-web component HTML to PolymerElement. It's better to be
   * replaced by polymer's $ helper dictionary.
   */
  private getRequiredElement<T extends HTMLElement = HTMLElement>(
      query: string): T {
    const el = this.root!.querySelector<T>(query);
    assert(el);
    assert(el instanceof HTMLElement);
    return el;
  }
}

/**
 * Creates closures to build the logs table row by row incrementally.
 * @param info The information that will be used to fill the table.
 */
export function createLogsMapTable(
    info: chrome.feedbackPrivate.LogsMapEntry[], root: ShadowRoot) {
  const logsMapTable = new LogsMapTable(root);

  for (const key in info) {
    const item = info[key]!;
    tableCreationClosuresQueue.push(
        logsMapTable.createTableRowWrapper(item['key'], item['value']));
  }

  tableCreationClosuresQueue.push(
      logsMapTable.finishPageLoading.bind(logsMapTable));

  logsMapTable.processQueue();
}
