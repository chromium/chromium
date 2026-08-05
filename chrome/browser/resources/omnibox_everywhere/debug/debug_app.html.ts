// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereDebugAppElement} from './debug_app.js';

export function getHtml(this: OmniboxEverywhereDebugAppElement) {
  return html`
<h1>Omnibox Everywhere Diagnostic</h1>
<div class="toggle-container">
  <label class="switch">
    <input id="bgModeToggle" type="checkbox" .checked="${this.bgModeEnabled}"
        @change="${this.onBgModeToggleChange}">
    <span class="slider"></span>
  </label>
  <span class="toggle-label">Enable Background Mode</span>
</div>
<div class="toggle-container">
  <label class="switch">
    <input id="hotkeyToggle" type="checkbox" .checked="${this.hotkeyEnabled}"
        @change="${this.onHotkeyToggleChange}">
    <span class="slider"></span>
  </label>
  <span class="toggle-label">Enable Global Hotkey</span>
</div>
<div class="invoke-container">
  <label for="sourceSelect" class="invoke-label">Invocation Source:</label>
  <select id="sourceSelect" class="invoke-select"
      .value="${String(this.selectedInvocationSource)}"
      @change="${this.onInvocationSourceChange}">
    ${this.computeInvocationSourceOptions().map(option => html`
      <option value="${option.value}"
          ?selected="${this.isSource(option.value)}">
        ${option.name} (${option.value})
      </option>
    `)}
  </select>
  <button class="invoke-button" @click="${this.onInvokeClick}">
    Invoke Omnibox Everywhere
  </button>
</div>`;
}
