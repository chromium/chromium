// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BaseDialogLearnMore} from './base_dialog_learn_more.js';

export function getHtml(this: BaseDialogLearnMore) {
  return html`<!--_html_template_start_-->
<div class="learn-more">
  <cr-expand-button ?expanded="${this.expanded_}"
      @expanded-changed="${this.onExpandedChanged_}">
    <div class="cr-secondary-text">${this.title}</div>
  </cr-expand-button>
  <cr-collapse id="collapse" ?opened="${this.expanded_}">
    <slot></slot>
  </cr-collapse>
</div>
<!--_html_template_end_-->`;
}
