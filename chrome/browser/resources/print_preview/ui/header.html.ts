// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HeaderElement} from './header.js';

export function getHtml(this: HeaderElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="headerContainer">
  <h1 class="title">$i18n{title}</h1>
  <cr-icon ?hidden="${!this.managed}" icon="print-preview:business"
       alt="" title="$i18n{managedSettings}">
  </cr-icon>
</div>
<span class="summary">${this.summary_}</span>
<!--_html_template_end_-->`;
  // clang-format on
}
