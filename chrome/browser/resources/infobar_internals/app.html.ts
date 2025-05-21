// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {InfobarInternalsAppElement} from './app.js';

export function getHtml(this: InfobarInternalsAppElement) {
  return html`<!--_html_template_start_-->
    <h1>Infobar Internals</h1>
    <ul>
      ${this.infobars.map(infobar => html`
            <li>
              <button data-type=${infobar.type} @click=${this.onTrigger}>
                ${infobar.name}
              </button>
            </li>`)}
    </ul>
    <!--_html_template_end_-->`;
}
