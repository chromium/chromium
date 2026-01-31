// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UnexportableKeysInternalsAppElement} from './app.js';

export function getHtml(this: UnexportableKeysInternalsAppElement) {
  return html`
<h1>Unexportable Keys Internals</h1>
<table>
  <tr>
    <th>Wrapped Key</th>
    <th>Algorithm</th>
    <th>Key Tag</th>
    <th>Creation Time</th>
    <th>Actions</th>
  </tr>
  ${this.unexportableKeysInfo_.map((item, index) => html`
    <tr>
      <td class="wrapped-key">${item.wrappedKey}</td>
      <td>${item.algorithm}</td>
      <td>${item.keyTag}</td>
      <td>${item.creationTime.toLocaleString()}</td>
      <td>
        <cr-icon-button class="icon-delete" iron-icon="cr:delete" title="Delete"
            data-index="${index}" @click="${this.onDeleteKeyClick_}">
        </cr-icon-button>
      </td>
    </tr>
  `)}
</table>
<cr-toast id="deleteErrorToast" duration="5000">
  <div>Error when deleting a key, please try again</div>
</cr-toast>`;
}
