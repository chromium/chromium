// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Result} from './launcher_internals.mojom-webui.js';

export interface LauncherResultsTableElement {
  $: {
    'headerRow': HTMLTableRowElement,
    'resultsSection': HTMLTableSectionElement,
  };
}

export class LauncherResultsTableElement extends PolymerElement {
  static get is() {
    return 'launcher-results-table';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  // Current results keyed by result id.
  private results: Map<string, Result> = new Map();

  // Extra header cells, keyed by their text content. These are placed into the
  // header row in insertion order.
  private headerCells: Map<string, HTMLTableCellElement> = new Map();

  clearResults() {
    this.results.clear();
    this.$.resultsSection.innerHTML = '';
    for (const cell of this.headerCells.values()) {
      this.$.headerRow.removeChild(cell);
    }
    this.headerCells.clear();
  }

  addResults(newResults: Array<Result>) {
    for (const result of newResults) {
      this.results.set(result.id, result);
      this.addHeaders(Object.keys(result.rankerScores));
    }

    // Clear and repopulate the table, sorted by descending score.
    // TODO(crbug.com/1211232): Allow sorting by other ranking methods.
    let sortedResults = Array.from(this.results.values());
    sortedResults.sort((a, b) => b.score - a.score);
    this.$.resultsSection.innerHTML = '';
    for (const result of sortedResults) {
      const newRow = this.$.resultsSection.insertRow();
      [result.id,
       result.title,
       result.description,
       result.resultType,
       result.displayType,
       result.score.toString(),
       ...this.flattenScores(result.rankerScores),
      ].forEach(field => {
        const newCell = newRow.insertCell();
        newCell.textContent = field;
      });
    }
  }

  // Appends any new headers to the end of the header row.
  private addHeaders(newHeaders: Array<string>) {
    for (const header of newHeaders) {
      if (this.headerCells.has(header)) {
        continue;
      }
      const newCell = this.$.headerRow.insertCell();
      newCell.textContent = header;
      this.headerCells.set(header, newCell);
    }
  }

  // Converts ranker scores into an array of scores in string form and ordered
  // according to the current headers.
  private flattenScores(inputScores: {[key: string]: number}): Array<string> {
    let outputScores = [];
    for (const header of this.headerCells.keys()) {
      const score = inputScores[header];
      outputScores.push(score === undefined ? '' : score.toString());
    }
    return outputScores;
  }
}

customElements.define(
    LauncherResultsTableElement.is, LauncherResultsTableElement);
