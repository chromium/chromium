// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxAimAppElement} from './aim_app.js';

export function getHtml(this: OmniboxAimAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="content">
  <cr-omnibox-composebox id="composebox"
      .closeOnEscape="${this.webuiOmniboxFullPopupEnabled_}"
      @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
      @close-composebox="${this.onCloseComposebox_}">
  </cr-omnibox-composebox>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
