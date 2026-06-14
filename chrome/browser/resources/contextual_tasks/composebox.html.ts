// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksComposeboxElement} from './composebox.js';

// clang-format off
export function getHtml(this: ContextualTasksComposeboxElement) {
  /* 'suggestions' ternary is to change DOM order:
   *  Side panel has suggestions appear between header and composebox.
   *  Full tab has suggestions appear below the composebox (which is below the header).
   */

  /*
   * TODO(crbug.com/486996060): make suggestions component
   * to dedupe logic
   */
  return html`<!--_html_template_start_-->
    ${this.isSidePanel && this.enableNativeZeroStateSuggestions ? html`
      <cr-composebox-dropdown
          id="contextualTasksSuggestionsContainer"
          role="listbox"
          .result="${this.zeroStateSuggestions_}"
          .maxSuggestions="${5}"
          .overrideClampLineNum="${3}"
          .selectedMatchIndex="${this.selectedMatchIndex_}"
          ?hidden="${!this.shouldShowSuggestions_()}"
          @match-focusin="${this.onMatchFocusin_}"
          @keydown="${this.onDropdownKeydown_}">
      </cr-composebox-dropdown>
      ${this.showSuggestionsActivityLink_ &&
          this.shouldShowSuggestions_() ? html`
        <div id="suggestionActivity">
          <localized-link
              .localizedString="${this.i18nAdvanced('suggestionActivityLink')}"
              @link-clicked="${this.onSuggestionActivityLinkClicked_}">
          </localized-link>
        </div>
      `: ''}
    ` : ''}
    <div id="composeboxContainer"
      style="
        --composebox-height: ${this.composeboxHeight_}px;
        --composebox-dropdown-height: ${this.composeboxDropdownHeight_}px;"
        >
      ${this.useFork_ ? html`
        <contextual-tasks-inner-composebox
          id="composebox"
          .isSidePanel="${this.isSidePanel}"
          .autofocus="${false}"
          carousel-on-top_
          entrypoint-name="ContextualTasks"
          searchbox-layout-mode="TallBottomContext"
          .lensButtonDisabled="${this.lensButtonDisabled_}"
          .showLensButton="${this.shouldShowLensButton_()}"
          .suggestionActivityEnabled="${false}"
          .disableCaretColorAnimation="${!this.caretAnimationsEnabled_}"
          .inputPlaceholderOverride="${this.getInputPlaceholder_()}"
          .dropdownNeeded="${this.isDropdownNeeded_()}"
          .lensButtonTriggersOverlay="${true}"
          .enableCarouselScrolling="${true}"
          .isFollowupQuery="${!this.isZeroState}"
          .enableFileHint="${this.enableFileHint_}"
          .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted()}"
          .clearAllInputsWhenSubmittingQuery="${true}"
          .showVoiceSearch="${true}"
          .usePecApi="${this.usePecApi_}"
          .smartTabSharingVisible="${this.smartTabSharingVisible_}"
          .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled_}"
          .energyEffectEnabled="${this.energyEffectAnimationEnabled_}"
          .glifAnimationState="${this.glifAnimationState_}"
          .disableFallbackGlifAnimation="${true}"
          .isZeroState="${this.isZeroState}"
          @result-changed="${this.onSuggestionsResultChanged_}"
          @open-image-upload="${this.onOpenImageUpload_}"
          @open-file-upload="${this.onOpenFileUpload_}"
          @input-state-changed="${this.onInputStateChanged_}"
          @show-suggestion-activity-link=
              "${this.onShowSuggestionActivityLink_}">
      </contextual-tasks-inner-composebox>
    ` : html`
      <cr-composebox
          id="composebox"
          .isSidePanel="${this.isSidePanel}"
          .autofocus="${false}"
          carousel-on-top_
          entrypoint-name="ContextualTasks"
          searchbox-layout-mode="TallBottomContext"
          .lensButtonDisabled="${this.lensButtonDisabled_}"
          .showLensButton="${this.shouldShowLensButton_()}"
          .suggestionActivityEnabled="${false}"
          .disableCaretColorAnimation="${!this.caretAnimationsEnabled_}"
          .inputPlaceholderOverride="${this.getInputPlaceholder_()}"
          .dropdownNeeded="${this.isDropdownNeeded_()}"
          .lensButtonTriggersOverlay="${true}"
          .enableCarouselScrolling="${true}"
          .isFollowupQuery="${!this.isZeroState}"
          .enableFileHint="${this.enableFileHint_}"
          .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted()}"
          .clearAllInputsWhenSubmittingQuery="${true}"
          .showVoiceSearch="${true}"
          .usePecApi="${this.usePecApi_}"
          .smartTabSharingVisible="${this.smartTabSharingVisible_}"
          .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled_}"
          .energyEffectEnabled="${this.energyEffectAnimationEnabled_}"
          .glifAnimationState="${this.glifAnimationState_}"
          .disableFallbackGlifAnimation="${true}"
          .isZeroState="${this.isZeroState}"
          @result-changed="${this.onSuggestionsResultChanged_}"
          @open-image-upload="${this.onOpenImageUpload_}"
          @open-file-upload="${this.onOpenFileUpload_}"
          @input-state-changed="${this.onInputStateChanged_}"
          @show-suggestion-activity-link=
              "${this.onShowSuggestionActivityLink_}">
      </cr-composebox>
    `}
    </div>
    ${!this.isSidePanel && this.enableNativeZeroStateSuggestions ? html`
      <cr-composebox-dropdown
          id="contextualTasksSuggestionsContainer"
          role="listbox"
          .result="${this.zeroStateSuggestions_}"
          .maxSuggestions="${5}"
          .overrideClampLineNum="${3}"
          .selectedMatchIndex="${this.selectedMatchIndex_}"
          ?hidden="${!this.shouldShowSuggestions_()}"
          @match-focusin="${this.onMatchFocusin_}"
          @keydown="${this.onDropdownKeydown_}">
      </cr-composebox-dropdown>
      ${this.showSuggestionsActivityLink_ &&
          this.shouldShowSuggestions_() ? html`
        <div id="suggestionActivity">
          <localized-link
              .localizedString="${this.i18nAdvanced('suggestionActivityLink')}"
              @link-clicked="${this.onSuggestionActivityLinkClicked_}">
          </localized-link>
        </div>
      `: ''}
    ` : ''}

  <!--_html_template_end_-->`;
}
// clang-format on
