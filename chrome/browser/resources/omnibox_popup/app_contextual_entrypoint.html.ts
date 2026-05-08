// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hasAllowedInputs} from '//resources/cr_components/composebox/common.js';
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupAppElement} from './app.js';

export function getHtml(this: OmniboxPopupAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="context-menu-container">
  ${this.shouldHideEntrypointButton_() ||
    !hasAllowedInputs(this.inputState_, this.usePecApi_) ? '' : html`
    <cr-composebox-contextual-entrypoint-button id="context"
        class="upload-button"
        .inputState="${this.inputState_}"
        ?show-suggestion-label="${this.showContextButtonSuggestionLabel_}"
        ?lens-chip-shown="${this.isLensChipShown_}"
        @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}">
    </cr-composebox-contextual-entrypoint-button>
  `}
  ${this.isContentSharingEnabled_ && this.computeShowRecentTabChip_() ? html`
    <composebox-recent-tab-chip id="recentTabChip"
        class="upload-button contextual-chip"
        .recentTab="${this.recentTabForChip_!}"
        @add-tab-context="${this.onAddTabContext_}">
    </composebox-recent-tab-chip>
  ` : nothing}
  ${this.isLensChipShown_ ? html`
    <cr-composebox-lens-search id="lensSearchChip"
        class="upload-button contextual-chip"
        @lens-search-click="${this.onLensSearchClick_}">
    </cr-composebox-lens-search>
  ` : nothing}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
