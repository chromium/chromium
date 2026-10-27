// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {MediaPickerElement} from './media_picker.js';

export function getHtml(this: MediaPickerElement) {
  return html`<!--_html_template_start_-->
    <div class="cr-row first" id="picker" hidden>
      <select id="mediaPicker" class="md-select" @change="${this.onChange_}"
          aria-label="${this.label}">
        ${this.devices.map(item => html`
          <option value="${item.id}">
            ${item.name}
          </option>
        `)}
      </select>
    </div>
<!--_html_template_end_-->`;
}
