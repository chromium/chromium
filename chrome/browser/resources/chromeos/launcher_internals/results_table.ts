// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Result} from './launcher_internals.mojom-webui.js';
import {getTemplate} from './results_table.html.js';

export interface LauncherResultsTableElement {
  $: {
    'headerRow': HTMLTableRowElement,
    'resultsSection': HTMLTableSectionElement,
    'displayScoreHeader': HTMLTableCellElement,
  };
}

export class LauncherResultsTableElement extends PolymerElement {
  static get is() {
    return 'launcher-results-table';
  }

  static get template() {
    return getTemplate();
  }

  // Current results keyed by result id.
  private results: Map<string, Result> = new Map();

  // Extra header cells, keyed by their text content. These are placed into the
  // header row in insertion order.
  private headerCells: Map<string, HTMLTableCellElement> = new Map();

  // The result property used to sort the table. 'Display score' is the default
  // key, and this will change whenever the user clicks on a new header to sort
  // by.
  private sortKey: string = 'Display score';

  // The IDs of results that are currently selected. This is used to persist
  // formatting when the table is sorted.
  private selectedIds: Set<string> = new Set();

  override connectedCallback() {
    super.connectedCallback();
    this.$.displayScoreHeader.addEventListener(
        'click',
        () => this.sortTable('Display score', /*resultsChanged=*/ false));
  }

  clearResults() {
    this.results.clear();
    this.$.resultsSection.innerHTML =
        window.trustedTypes ? (window.trustedTypes.emptyHTML) : '';
    for (const cell of this.headerCells.values()) {
      this.$.headerRow.removeChild(cell);
    }
    this.headerCells.clear();
  }

  addResults(newResults: Result[]) {
    for (const result of newResults) {
      this.results.set(result.id, result);
      this.addHeaders(Object.keys(result.rankerScores));
    }
    this.sortTable(this.sortKey, /*resultsChanged=*/ true);
  }

  // Appends any new headers to the end of the header row. All new headers
  // should support sort-on-click.
  private addHeaders(newHeaders: string[]) {
    for (const header of newHeaders) {
      if (this.headerCells.has(header)) {
        continue;
      }
      const newCell = this.$.headerRow.insertCell();
      newCell.textContent = header;
      newCell.className = 'sort-header';
      newCell.addEventListener(
          'click', () => this.sortTable(header, /*resultsChanged=*/ false));
      this.headerCells.set(header, newCell);
    }
  }

  // Repopulates the table with results sorted by the current key in descending
  // order.
  private sortTable(sortKey: string, resultsChanged: boolean) {
    if (!resultsChanged && this.sortKey === sortKey) {
      return;
    }
    this.sortKey = sortKey;

    const sortedResults = Array.from(this.results.values());
    if (this.sortKey === 'Display score') {
      sortedResults.sort((a, b) => b.score - a.score);
    } else {
      const getSortValue = (result: Result): number => {
        const value = result.rankerScores[this.sortKey];
        return value === undefined ? 0 : value;
      };
      sortedResults.sort((a, b) => getSortValue(b) - getSortValue(a));
    }

    // Clear and repopulate the results table.
    this.$.resultsSection.innerHTML =
        window.trustedTypes ? (window.trustedTypes.emptyHTML) : '';
    for (const result of sortedResults) {
      const newRow = this.$.resultsSection.insertRow();
      newRow.addEventListener('click', (e: Event) => this.toggleRowSelected(e));
      if (this.selectedIds.has(result.id)) {
        newRow.classList.add('selected');
      }
      [result.id,
       result.title,
       result.description,
       result.resultType,
       result.metricsType,
       result.displayType,
       result.score.toString(),
       ...this.flattenScores(result.rankerScores),
      ].forEach(field => {
        const newCell = newRow.insertCell();
        newCell.textContent = field;
      });
    }
  }

  // Converts ranker scores into an array of scores in string form and ordered
  // according to the current headers.
  private flattenScores(inputScores: {[key: string]: number}): string[] {
    const outputScores = [];
    for (const header of this.headerCells.keys()) {
      const score = inputScores[header];
      outputScores.push(score === undefined ? '' : score.toString());
    }
    return outputScores;
  }

  // Toggles selection in the class list of the targeted row.
  private toggleRowSelected(event: Event) {
    const row = (event.target as HTMLElement).closest('tr');
    if (row == null || row.cells.length === 0) {
      return;
    }

    const id = row.cells[0]!.textContent!;
    if (row.classList.contains('selected')) {
      row.classList.remove('selected');
      this.selectedIds.delete(id);
    } else {
      row.classList.add('selected');
      this.selectedIds.add(id);
    }
  }
}

customElements.define(
    LauncherResultsTableElement.is, LauncherResultsTableElement);
