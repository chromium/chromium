// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Result} from './launcher_internals.mojom-webui.js';

export interface LauncherResultsTableElement {
  $: {
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

  private idSet: Set<string> = new Set();

  clearResults() {
    this.idSet.clear();
    this.$.resultsSection.innerHTML = '';
  }

  addResults(results: Array<Result>) {
    for (const result of results) {
      if (this.idSet.has(result.id)) {
        continue;
      }
      this.idSet.add(result.id);

      const newRow = this.$.resultsSection.insertRow();
      [result.id,
       result.title,
       result.description,
       result.resultType,
       result.displayType,
       result.score.toString(),
      ].forEach(field => {
        const newCell = newRow.insertCell();
        newCell.textContent = field;
      });

      // TODO(crbug.com/1211232): Handle other ranker scores and allow sorting
      // by the chosen score.
    }
  }
}

customElements.define(
    LauncherResultsTableElement.is, LauncherResultsTableElement);
