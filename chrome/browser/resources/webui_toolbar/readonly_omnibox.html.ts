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
  <div id="textContainer">${
    this.omniboxViewState.textPieces.map(
      ReadonlyOmniboxElement.renderTextPiece)
  }</div>
  <input id="textInput">
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
