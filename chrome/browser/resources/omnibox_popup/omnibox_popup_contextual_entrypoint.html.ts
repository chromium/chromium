// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hasAllowedInputs} from '//resources/cr_components/composebox/common.js';
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupContextualEntrypointElement} from './omnibox_popup_contextual_entrypoint.js';

export function getHtml(this: OmniboxPopupContextualEntrypointElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="context-menu-container">
  ${this.shouldHideEntrypointButton_() ||
      !hasAllowedInputs(this.inputState, this.usePecApi_) ? '' : html`
    <omnibox-popup-contextual-entrypoint-button id="context"
        class="upload-button"
        .inputState="${this.inputState}"
        .applyContextButtonBackground="${this.applyContextButtonBackground_}"
        .isOblongShape="${this.isOblongShape_}"
        ?show-suggestion-label="${this.showContextButtonSuggestionLabel_}">
    </omnibox-popup-contextual-entrypoint-button>
  `}
  ${this.isCurrentTabChipShown_ ? html`
    <composebox-current-tab-chip id="currentTabChip"
        class="upload-button contextual-chip"
        .currentTab="${this.currentTabForChip_!}"
        @add-tab-context="${this.onAddTabContext_}">
    </composebox-current-tab-chip>
  ` : nothing}
  ${this.isLensChipShown_ ? html`
    <cr-composebox-lens-search id="lensSearchChip"
        class="upload-button contextual-chip"
        @lens-search-click="${this.onLensSearchClick_}">
    </cr-composebox-lens-search>
  ` : nothing}
  ${this.isLensIconShown_ ? html`
    <cr-composebox-lens-search id="lensSearchIcon"
        is-icon
        class="upload-button"
        @lens-search-click="${this.onLensSearchClick_}">
    </cr-composebox-lens-search>
  ` : nothing}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
