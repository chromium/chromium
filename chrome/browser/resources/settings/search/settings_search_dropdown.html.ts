// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchDropdownElement} from './settings_search_dropdown.js';

export function getHtml(this: SettingsSearchDropdownElement) {
  return html`<!--_html_template_start_-->
<div id="dropdown" ?hidden="${!this.results_.length}">
  ${this.results_.map(item => html`
    <div class="result">
      <cr-icon class="result-icon" icon="${this.getResultIcon_(item)}"
          aria-hidden="true">
      </cr-icon>
      <div class="result-text">
        <div class="result-title">${item.title}</div>
        ${item.breadcrumbs.length ? html`
          <div class="result-breadcrumbs cr-secondary-text">
            ${item.breadcrumbs.join(' > ')}
          </div>
        ` : ''}
      </div>
    </div>
  `)}
</div>
<!--_html_template_end_-->`;
}
