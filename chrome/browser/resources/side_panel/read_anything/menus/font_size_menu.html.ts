// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FontSizeMenuElement} from './font_size_menu.js';

export function getHtml(this: FontSizeMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-lazy-render-lit id="lazyMenu" .template='${() => html`
  <cr-action-menu @keydown="${this.onKeydown_}"
      accessibility-label="$i18n{fontSizeTitle}"
      role-description="$i18n{menu}">
    <cr-icon-button class="font-size" role="menuitem"
        id="font-size-decrease"
        aria-label="$i18n{decreaseFontSizeLabel}"
        title="$i18n{decreaseFontSizeLabel}"
        iron-icon="${this.webuiRoundedIconsEnabled_
            ? 'read-anything:remove'
            : 'read-anything:font-size-decrease-old'}"
        @click="${this.onDecreaseClick_}">
    </cr-icon-button>
    <cr-icon-button class="font-size" role="menuitem"
        id="font-size-increase"
        aria-label="$i18n{increaseFontSizeLabel}"
        title="$i18n{increaseFontSizeLabel}"
        iron-icon="cr:add"
        @click="${this.onIncreaseClick_}">
    </cr-icon-button>
    <cr-button role="menuitem"
        id="font-size-reset"
        ?disabled="${this.isFontSizeDefault_()}"
        aria-label="$i18n{fontResetTooltip}"
        title="$i18n{fontResetTooltip}"
        @click="${this.onResetClick_}">
      $i18n{fontResetTitle}
    </cr-button>
    <div id="size-announce" class="announce-block"
        aria-live="polite" aria-relevant="additions"></div>
  </cr-action-menu>
`}'>
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
