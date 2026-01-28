// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WhatsNewAppElement} from './whats_new_app.js';

export function getHtml(this: WhatsNewAppElement) {
  // clang-format off
  return this.url_ ? html`<!--_html_template_start_-->
    <iframe id="content" src="${this.url_}"></iframe>
    ${this.isStaging_ ? html`<div id="staging-indicator">Staging</div>` : ''}
  <!--_html_template_end_-->` : '';
  // clang-format on
}
