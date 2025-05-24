// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceTranslationInternalsAppElement} from './app.js';

export function getHtml(this: OnDeviceTranslationInternalsAppElement) {
  return html`
<h1>On Device Translation Internals</h1>
<h2>Language Packages</h2>
<table class="package-table">
${this.languagePackStatus_.map((item, index) => html`
  <tr class="package-tr">
    <th class="package-name">${item.name}</th>
    <td class="package-status">${this.getStatusString_(item.status)}</td>
    <td>
      <button @click="${this.onButtonClick_}"
        data-index="${index}">${this.getButtonString_(item.status)}</button>
    </td>
  </tr>`)}
</table>
`;
}
