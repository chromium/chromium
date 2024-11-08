// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {KeyboardShortcutsElement} from './keyboard_shortcuts.js';

export function getHtml(this: KeyboardShortcutsElement) {
  // clang-format off
  return html`<!--_html_template_start_--><div id="container">
  ${this.calculateShownItems_().map(item => html`
    <div class="shortcut-card">
      <div class="card-title cr-title-text">
        <img class="icon" src="${item.iconUrl}" alt="">
        <span role="heading" aria-level="2">${item.name}</span>
      </div>
      <div class="card-controls">
        ${item.commands.map(command => html`
          <div class="command-entry">
            <span class="command-name">${command.description}</span>
            <extensions-shortcut-input .delegate="${this.delegate}"
                .item="${item}" .shortcut="${command.keybinding}"
                .command="${command}">
            </extensions-shortcut-input>
            <select class="md-select" @change="${this.onScopeChanged_}"
                data-extension-id="${item.id}"
                data-command-name="${command.name}"
                aria-label="${this.computeScopeAriaLabel_(item, command)}"
                ?disabled="${this.computeScopeDisabled_(command)}"
                .value="${command.scope}">
              <option value="${chrome.developerPrivate.CommandScope.CHROME}">
                $i18n{shortcutScopeInChrome}
              </option>
              <option value="${chrome.developerPrivate.CommandScope.GLOBAL}">
                $i18n{shortcutScopeGlobal}
              </option>
            </select>
          </div>`)}
      </div>
    </div>`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
