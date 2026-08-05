// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksAppElement} from './app.js';

// clang-format off
export function getHtml(this: ContextualTasksAppElement) {
  return html`<!--_html_template_start_-->
<if expr="not is_android">
  <link rel="stylesheet" href="layout_constants.css">
</if>
  ${this.isShownInTab_ ? '' : html`
    <div id="toolbarOverlay">
      <top-toolbar id="toolbar"
          .title="${this.threadTitle_}"
          .darkMode="${this.darkMode_}"
          .isAiPage="${this.isAiPage_}"
          .isAimEligible="${this.isAimEligible_}"
          .isUserSignedIn="${this.isUserSignedIn_}"
          .enableOpenInNewTabButton="${this.isAiPage_ && !this.isErrorPageVisible_}"
          .onboardingTooltipShowing="${this.onboardingTooltipShowing_}"
          .lensSearchTooltipShowing="${this.lensSearchTooltipTarget_ !== null}"
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
    <contextual-tasks-info-tooltip id="lensSearchTooltip"
        .target="${this.lensSearchTooltipTarget_}"
        .container="${this.composeboxElement_}"
        title-text="$i18n{lensSearchTooltipTitle}"
        body-text="$i18n{lensSearchTooltipBody}"
        close-button-type="icon"
        horizontal-align="right"
        @tooltip-dismissed="${this.onLensSearchTooltipDismissed_}">
    </contextual-tasks-info-tooltip>

    ${this.showSmartTabSharingTryItIph_ ? html`
      <contextual-tasks-banner-promo id="stsTryItPromo"
          style="${this.getBannerPromoBoundsStyles_()}"
          accept-button-text="$i18n{stsTryItTurnOn}"
          dismiss-button-text="$i18n{stsTryItNotNow}"
          @dismiss="${this.onStsTryItDismiss_}"
          @accept="${this.onStsTryItAccept_}">
        <span slot="header">$i18n{stsTryItHeader}</span>
        <span slot="body">$i18n{stsTryItBody}</span>
      </contextual-tasks-banner-promo>
    ` : ''}
    ${this.showSmartTabSharingDefaultOnIph_ ? html`
      <contextual-tasks-banner-promo id="stsDefaultOnPromo"
          style="${this.getBannerPromoBoundsStyles_()}"
          accept-button-text="$i18n{stsDefaultOnTurnOn}"
          dismiss-button-text="$i18n{stsDefaultOnNotNow}"
          @dismiss="${this.onStsDefaultOnDismiss_}"
          @accept="${this.onStsDefaultOnAccept_}">
        <span slot="header">$i18n{stsDefaultOnHeader}</span>
        <span slot="body">
          $i18n{stsDefaultOnBody}
          <a href="chrome://settings/ai"
              target="_blank">$i18n{stsDefaultOnLink}</a>
          $i18n{stsDefaultOnBodyEnd}
        </span>
      </contextual-tasks-banner-promo>
    ` : ''}
</if>
<if expr="not is_android or enable_webui_contextual_tasks_composebox">


    <contextual-tasks-composebox id="composebox"
          style="${this.getComposeboxBoundsStyles()}"
          ?hidden="${this.isComposeboxHidden_()}"
          .isZeroState="${this.isZeroState_}"
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
<if expr="not is_android or enable_webui_contextual_tasks_composebox">
  <!-- Placed at the top level to ensure it sits on top of all other elements in the z-order stacking context, avoiding confinement by container boundaries or lower z-index stacking contexts (like #flexCenterContainer). -->
  ${this.showOnboardingTooltip_ ? html`
    <contextual-tasks-onboarding-tooltip id="onboardingTooltip"
        @onboarding-tooltip-dismissed="${this.onOnboardingTooltipDismissed_}">
    </contextual-tasks-onboarding-tooltip>
  ` : ''}
</if>
<if expr="not is_android">
    <contextual-tasks-info-tooltip id="askGTooltip"
        .target="${this.askGTooltipTarget_}"
        .container="${this.composeboxElement_}"
        title-text="$i18n{askGFirstRunTitle}"
        body-text="$i18n{askGFirstRunBody}"
        close-button-type="icon"
        horizontal-align="left"
        @tooltip-dismissed="${this.onAskGTooltipDismissed_}">
    </contextual-tasks-info-tooltip>
</if>
  <!--_html_template_end_-->`;
}
// clang-format on

/* TODO(crbug.com/470105276): Put composebox into composebox
 * slot for flexbox center formatting instead of temp formatting.
 */
