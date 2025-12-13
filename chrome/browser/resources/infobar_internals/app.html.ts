// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {InfobarInternalsAppElement} from './app.js';

export function getHtml(this: InfobarInternalsAppElement) {
  return html`<!--_html_template_start_-->
    <div class="container">
      <h1>Infobar Trigger Page</h1>
      <table class="infobar-table">
        <thead>
          <tr>
            <th>Name</th>
            <th>Description</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody>
          ${this.infobars.map(infobar => html`
            <tr>
              <td>${infobar.name}</td>
              <td class="description">${infobar.description}</td>
              <td>
                <button data-type="${infobar.type}" @click="${this.onTrigger}">
                  Trigger
                </button>
              </td>
            </tr>
          `)}
        </tbody>
      </table>
    </div>
    <!--_html_template_end_-->`;
}
