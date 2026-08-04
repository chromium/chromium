// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupContextualEntrypointButtonElement} from './omnibox_popup_contextual_entrypoint_button.js';

export function getHtml(this: OmniboxPopupContextualEntrypointButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-composebox-contextual-entrypoint-button
        exportparts="entrypoint-button, context-menu-entrypoint-icon"
        .inputState="${this.inputState}"
        .applyContextButtonBackground="${this.applyContextButtonBackground}"
        .isOblongShape="${this.isOblongShape}"
        ?show-suggestion-label="${this.showSuggestionLabel}"
        .hasPopupFocus="${this.hasPopupFocus}"
        @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}">
    </cr-composebox-contextual-entrypoint-button>
<!--_html_template_end_-->`;
  // clang-format on
}
