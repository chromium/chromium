// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsDropdownMenuElement} from './settings_dropdown_menu.js';

export function getHtml(this: SettingsDropdownMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.pref?.controlledBy ? html`
  <cr-policy-pref-indicator .pref="${this.pref}"></cr-policy-pref-indicator>
` : ''}
<select class="md-select" id="dropdownMenu" @change="${this.onChange_}"
    aria-label="${this.label}"
    aria-description="${this.subLabel || nothing}" part="select"
    ?disabled="${this.shouldDisableMenu_()}">
  ${this.menuOptions.map(item => html`
    <option value="${item.value}" ?hidden="${!!item.hidden}"
        search-hint="${item.searchHint || ''}">
      ${item.name}
    </option>
  `)}
  <option value="${this.notFoundValue}"
      ?disabled="${!this.showNotFoundValue_()}">
    $i18n{custom}
  </option>
</select>
<!--_html_template_end_-->`;
  // clang-format on
}
