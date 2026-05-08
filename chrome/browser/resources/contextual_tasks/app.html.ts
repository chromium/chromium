// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksAppElement} from './app.js';

// clang-format off
export function getHtml(this: ContextualTasksAppElement) {
  return html`<!--_html_template_start_-->
  ${this.isShownInTab_ ? '' : html`
    <div id="toolbarOverlay">
      <top-toolbar id="toolbar"
          .title="${this.threadTitle_}"
          .darkMode="${this.darkMode_}"
          .isAiPage="${this.isAiPage_}"
          .enableOpenInNewTabButton="${this.isAiPage_ && !this.isErrorPageVisible_}"
          @new-thread-click="${this.onNewThreadClick_}">
      </top-toolbar>
    </div>
  `}
  <webview id="threadFrame" allowtransparency="on"
      partition="persist:contextual-tasks"
      style="${this.getThreadFrameStyles()}">
  </webview>
  <ghost-loader id="ghostLoader"></ghost-loader>
  ${this.isErrorDialogVisible_ ?
    html`<contextual-tasks-error-dialog></contextual-tasks-error-dialog>` : ''}
  <div id="flexCenterContainer">
    <div id="composeboxHeaderWrapper"
        ?hidden="${this.isComposeboxHeaderWrapperHidden_()}">
      <h1 class="thread-header" id="composeboxHeader">
        ${this.userName_
            ? [
              html`<span>${this.friendlyZeroStateTitleBeforeName_}</span>`,
              html`<span id="nameShimmer" class="name-shimmer">${this.userName_}</span>`,
              html`<span>${this.friendlyZeroStateTitleAfterName_}</span>`,
            ]
            : html`<span>${this.friendlyZeroStateTitle}</span>`
        }
        ${this.friendlyZeroStateSubtitle.length > 0 ?
            html`<br>
            ${this.friendlyZeroStateSubtitle}` : ''}
      </h1>
    </div>
<if expr="not is_android">
    ${this.showOnboardingTooltip_ ? html`
      <contextual-tasks-onboarding-tooltip id="onboardingTooltip">
      </contextual-tasks-onboarding-tooltip>
    ` : ''}
    <contextual-tasks-composebox id="composebox"
          style="${this.getComposeboxBoundsStyles()}"
          ?hidden="${this.isComposeboxHidden_()}"
          .isZeroState="${!!this.isZeroState_}"
          .isSidePanel="${!this.isShownInTab_}"
          .isLensOverlayShowing="${this.isLensOverlayShowing_}"
          .isOverlayOpenForAimVisualSearch="${
              this.isOverlayOpenForAimVisualSearch_}"
          .enableNativeZeroStateSuggestions="${
              this.enableNativeZeroStateSuggestions_}"
          .inputEnabled="${!this.isInputLocked_}"
          .inNlm="${this.inNlm_}">
    </contextual-tasks-composebox>
</if>
  </div>
  <error-page id="errorPage"></error-page>
  <div id="iphMenuSmartTabSharingAnchor"></div>
  <!--_html_template_end_-->`;
}
// clang-format on

/* TODO(crbug.com/470105276): Put composebox into composebox
 * slot for flexbox center formatting instead of temp formatting.
 */
