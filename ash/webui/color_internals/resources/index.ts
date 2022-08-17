// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CROS_TOKENS_JSON_URL = 'color_internals_tokens.json';

interface Color {
  value: string;
  formula: string;
}

interface Token {
  name: string;
  mode_values: {light: Color, dark: Color};
}

function createColumnSpanningCell(text: string): HTMLTableCellElement {
  const cell = document.createElement('td');
  cell.scope = 'col';
  cell.textContent = text;
  return cell;
}

function appendTokenRowToTable(
    table: HTMLTableSectionElement, token: Token): void {
  const newRow = table.insertRow();

  function appendColorCell(color: string) {
    const cell = createColumnSpanningCell(color);
    cell.classList.add('monospace');
    const colorSwatch = document.createElement('span');
    colorSwatch.classList.add('color-swatch');
    colorSwatch.style.backgroundColor = color;
    cell.appendChild(colorSwatch);
    newRow.append(cell);
  }

  function appendFormulaCell(formula: string) {
    const cell = createColumnSpanningCell(formula);
    newRow.append(cell);
  }

  const header = document.createElement('td');
  header.textContent = token.name;
  header.classList.add('monospace');
  newRow.append(header);

  const {light, dark} = token.mode_values;
  appendColorCell(light.value);
  appendFormulaCell(light.formula);
  appendColorCell(dark.value);
  appendFormulaCell(dark.formula);
}

// The list of tokens and color values are provided as JSON resource by the
// backend.
// TODO(b/222408581): re-request JSON and re-render table on themeChanged().
async function requestJSON(): Promise<JSON> {
  return new Promise(function(resolve, reject) {
    const xhr = new XMLHttpRequest();
    xhr.open('GET', CROS_TOKENS_JSON_URL, true);
    xhr.responseType = 'json';
    xhr.onload = () => {
      resolve(xhr.response);
    };
    xhr.onerror = () => {
      reject(xhr.response);
    };
    xhr.send();
  });
}

async function populateTokenTable() {
  const table = (document.querySelector('table')!).querySelector('tbody')!;
  const json = await requestJSON();
  Object.values(json).forEach(t => appendTokenRowToTable(table, t as Token));
}

window.onload = () => {
  populateTokenTable();
};
