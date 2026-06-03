// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ReadonlyOmniboxElement} from './readonly_omnibox.js';

export function getHtml(this: ReadonlyOmniboxElement) {
  // clang-format off
  // This avoids any whitespace text nodes floating around that can confuse
  // things. The wrapper has tabindex of -1 since it should be skipped in
  // tab order, but should be able to get focus to forward it.
  return html`<!--_html_template_start_-->
<div id="textContainerWrap" tabindex="-1">
  <!-- Only one of #textInput and #textContainer is visible at once
       (by painting them on opaque backgrounds and altering z-index).
       textInput is out of normal flow (absolutely positioned) and sized to 100%
       of allocated width; #textContainer is in normal flow and sized based on
       contents.

       #textContainer contains richtext version of the input thus far;
       #textInput contains plaintext version of the input plus optionally an
       inline autocompletion rendered as selection.
   -->
  <input id="textInput">
  <!-- custom formatting/long line to prevent whitespace below -->
  <div id="textContainer">${
    this.omniboxViewState.textPieces.map(
      item => html`<span
          class="${ReadonlyOmniboxElement.getTextPieceClasses(item)}">${item.text}</span>`)
  }</div>
  <!-- #inlineAutocomplete is used to position #additionalText to the
    right of both the text and the inline completion within the
    #textInput -->
  <span id="inlineAutocomplete">${
        this.omniboxViewState.inlineAutocompletion}</span>
  <span id="additionalText">${this.omniboxViewState.additionalText}</span>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
