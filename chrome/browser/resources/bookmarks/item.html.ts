// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksItemElement} from './item.js';

export function getHtml(this: BookmarksItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="icon"></div>
<div id="website-text" role="gridcell">
  <div id="website-title" class="elided-text" title="${this.getItemTitle_()}">
    ${this.getItemTitle_()}
  </div>
  <div id="website-url" class="elided-text" title="${this.getItemUrl_()}">
    ${this.getItemUrl_()}
  </div>
</div>
<div role="gridcell">
  <cr-icon-button class="icon-more-vert"
      id="menuButton"
      tabindex="${this.ironListTabIndex}"
      title="$i18n{moreActionsButtonTitle}"
      aria-label="${this.getButtonAriaLabel_()}"
      @click="${this.onMenuButtonClick_}"
      aria-haspopup="menu">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
