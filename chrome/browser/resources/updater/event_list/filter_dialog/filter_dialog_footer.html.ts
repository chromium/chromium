// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FilterDialogFooterElement} from './filter_dialog_footer.js';

export function getHtml(this: FilterDialogFooterElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<div class="filter-menu-footer">
  <cr-button class="cancel-button" @click="${this.onCancelClick}">
    $i18n{cancel}
  </cr-button>
  <cr-button class="action-button" @click="${this.onApplyClick}">
    $i18n{apply}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
