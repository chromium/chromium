// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';

import {getRGBAFromComputedStyle} from './utils.js';
import {startObservingWallpaperColors} from './wallpaper_colors.js';

const CROS_TOKENS_JSON_URL = 'color_internals_tokens.json';

interface Token {
  token_name: string;
  css_variable: string;
  mode_values?: {light: string, dark: string};
}

interface TokenArray {
  ref_tokens: Token[];
  sys_tokens: Token[];
}

function appendTokenRowToTable(
    table: HTMLTableSectionElement, token: Token): void {
  const newRow = table.insertRow();

  function appendColorCell(color: string) {
    const colorSwatch = document.createElement('span');
    const cell = document.createElement('td');
    const text = document.createElement('span');
    colorSwatch.classList.add('color-swatch');
    colorSwatch.style.backgroundColor = color;
    cell.classList.add('monospace');
    cell.appendChild(colorSwatch);
    newRow.append(cell);

    // Perform this step last so the style has actually been computed.
    text.textContent = getRGBAFromComputedStyle(colorSwatch);
    cell.appendChild(text);
  }

  function appendFormulaCell(formula: string) {
    const cell = document.createElement('td');
    cell.textContent = formula;
    newRow.append(cell);
  }

  const header = document.createElement('td');
  header.textContent = token.token_name;
  header.classList.add('monospace');
  newRow.append(header);

  const cssVariableString = `var(${token.css_variable})`;
  appendColorCell(cssVariableString);

  // cros.sys.* tokens contain a definition of how they were calculated.
  if (token.mode_values) {
    appendFormulaCell(token.mode_values.light);
    appendFormulaCell(token.mode_values.dark);
  }
}

// The list of tokens and color values are provided as JSON resource by the
// backend.
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
  function addTokens(table: HTMLTableSectionElement, tokens: Token[]) {
    Object.values(tokens).forEach(
        t => appendTokenRowToTable(table, t as Token));
  }
  const json = await requestJSON();
  const tokens = json as unknown as TokenArray;
  const refTable =
      (document.querySelector('table#ref-tokens')!).querySelector('tbody')!;
  const sysTable =
      (document.querySelector('table#sys-tokens')!).querySelector('tbody')!;
  addTokens(refTable, tokens.ref_tokens);
  addTokens(sysTable, tokens.sys_tokens);
}

function onColorChange() {
  const formatter = new Intl.DateTimeFormat('en', {
    hour: 'numeric',
    minute: 'numeric',
    second: 'numeric',
  });
  const span = document.querySelector<HTMLElement>('#last-updated');
  assert(span);
  span.innerText = formatter.format(new Date());
}

window.onload = () => {
  populateTokenTable();
  ColorChangeUpdater.forDocument().start();
  startObservingWallpaperColors();
  ColorChangeUpdater.forDocument().eventTarget.addEventListener(
      COLOR_PROVIDER_CHANGED, onColorChange);
  onColorChange();
};
