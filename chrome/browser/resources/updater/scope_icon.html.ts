// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ScopeIconElement} from './scope_icon.js';

export function getHtml(this: ScopeIconElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
${this.scope !== undefined ? html`
  <cr-icon icon="${this.icon}" title="${this.label}">
  </cr-icon>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
