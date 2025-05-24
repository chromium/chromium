// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsCodeSectionElement} from './code_section.js';

export function getHtml(this: ExtensionsCodeSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="scroll-container" ?hidden="${!this.highlighted_}" dir="ltr">
  <div id="main">
    <!-- Line numbers are not useful to a screenreader -->
    <div id="line-numbers" aria-hidden="true">
      <div class="more-code before" ?hidden="${!this.truncatedBefore_}">
        ...
      </div>
      <span>${this.lineNumbers_}</span>
      <div class="more-code after" ?hidden="${!this.truncatedAfter_}">
        ...
      </div>
    </div>
    <div id="source">
      <div class="more-code before" ?hidden="${!this.truncatedBefore_}">
        ${this.getLinesNotShownLabel_(this.truncatedBefore_)}
      </div>
      <span><!-- Whitespace is preserved in this span. Ignore new lines.
        --><span>${this.before_}</span><!--
        --><mark aria-description="${this.highlightDescription_}"><!--
          -->${this.highlighted_}<!--
        --></mark><!--
        --><span>${this.after_}</span><!--
      --></span>
      <div class="more-code after" ?hidden="${!this.truncatedAfter_}">
        ${this.getLinesNotShownLabel_(this.truncatedAfter_)}
      </div>
    </div>
  </div>
</div>
<div id="no-code" ?hidden="${!this.shouldShowNoCode_()}">
  ${this.couldNotDisplayCode}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
