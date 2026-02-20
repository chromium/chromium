// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ErrorPageElement} from './error_page.js';

export function getHtml(this: ErrorPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="mainContent">
  <div id="header">
    ${this.shouldShowErrorIcon() ? html`
      <cr-icon icon="cr:error-outline"></cr-icon>` : ''}
    <h1 class="headline">${this.errorTitle()}</h1>
  </div>
  <p class="body-text">${this.errorDescription()}</p>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
