// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToolChipsElement} from './tools.js';

export function getHtml(this: ToolChipsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="showToggleContainer" class="sp-card-content"
    @click="${this.onShowToggleClick_}">
  <div id="showChipsTitle">$i18n{showChipsToggleTitle}</div>
  <cr-toggle id="showChipsToggle" title="$i18n{showChipsToggleTitle}"
      ?checked="${this.isChipsEnabled_}" @change="${this.onShowToggleChange_}">
  </cr-toggle>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
