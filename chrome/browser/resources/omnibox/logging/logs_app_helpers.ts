// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function renderDropdown(
    filterName: string, uniqueItems: Set<string>, selectedItems: string[],
    changeHandler: (e: Event) => void) {
  return html`
    <div class="dropdown-content">
      ${[...uniqueItems].sort().map(item => html`
        <label>
          <input type="checkbox" value="${item}"
                 data-filter="${filterName}"
                 ?checked="${selectedItems.includes(item)}"
                 @change="${changeHandler}">
          ${item}
        </label>
      `)}
    </div>
  `;
}
