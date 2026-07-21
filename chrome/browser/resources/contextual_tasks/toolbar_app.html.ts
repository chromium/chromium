// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksToolbarAppElement} from './toolbar_app.js';

export function getHtml(this: ContextualTasksToolbarAppElement) {
  return html`<!--_html_template_start_-->
    <top-toolbar id="toolbar"
        .title="${this.threadTitle_}"
        .darkMode="${this.darkMode_}"
        .isAiPage="${this.isAiPage_}"
        .isAimEligible="${this.isAimEligible_}"
        .isUserSignedIn="${this.isUserSignedIn_}"
        .enableOpenInNewTabButton="${this.isAiPage_}"
        .onboardingTooltipShowing="${this.onboardingTooltipShowing_}"
        .lensSearchTooltipShowing="${this.lensSearchTooltipShowing_}"
        @new-thread-click="${this.onNewThreadClick_}">
    </top-toolbar>
  <!--_html_template_end_-->`;
}
