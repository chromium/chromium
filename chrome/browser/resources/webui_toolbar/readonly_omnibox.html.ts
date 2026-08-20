// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {ReadonlyOmniboxElement} from './readonly_omnibox.js';

export function getHtml(this: ReadonlyOmniboxElement) {
  // clang-format off
  // This avoids any whitespace text nodes floating around that can confuse
  // things. The wrapper has tabindex of -1 since it should be skipped in
  // tab order, but should be able to get focus to forward it.
  return html`<!--_html_template_start_-->
<div id="textContainerWrap" tabindex="-1">
  <!-- Only one of #textInput and #textContainer is visible at once
       (by controlling their opacity).

       textInput is out of normal flow (absolutely positioned) and sized to
       100% of allocated width; #textContainer is in normal flow and sized
       based on contents.

       #textContainer contains richtext version of the input thus far;
       #textInput contains plaintext version of the input plus optionally an
       inline autocompletion rendered as selection.
   -->
  <cr-searchbox-input id="textInput"
      .placeholderText="${this.getInputPlaceholder_()}"
      class="${this.getInputClasses_() ?? nothing}"
      @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated_}">
  </cr-searchbox-input>
  <!-- custom formatting/long line to prevent whitespace below -->
  <div id="textContainer" aria-hidden='true'>${
    this.omniboxViewState.textPieces.map(
      item => html`<span
          class="${ReadonlyOmniboxElement.getTextPieceClasses(item)}">${item.text}</span>`)
  }</div>
  <!-- #inlineAutocomplete has two possible uses:
    1. If the inline suggestion is rendered by <input>, it's is used to position
       #additionalText to the right of both the text and the inline completion.
       In that case, it's invisible.
    2. In case there is an IME composition going on, it does actually render
       the suggestion; this is done since trying to render it with selection
       would mess up the IME.

    The composing attribute distinguishes the two cases. -->
  <span id="inlineAutocomplete" ?composing="${this.isComposing}"
        aria-live="polite"
        aria-hidden="${this.isComposing ? 'false' : 'true'}">${
        this.omniboxViewState.inlineAutocompletion}</span>
  <span id="additionalText">${this.omniboxViewState.additionalText}</span>

  <!-- We need to temporarily transfer ARIA focus to here via
       ariaActiveDescendant to get ariaNotify to win over updates over the
       <input> proper -->
  <div id="announcementDistraction">${
    this.omniboxViewState.a11yFriendlySuggestionText}</div>
</div>

<div id="dragTemplate" aria-hidden="true">
  <img id="dragFavicon" src="${this.getDragFaviconUrl_()}" alt="">
  <span id="dragTitle">${this.getDragTitle_()}</span>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
