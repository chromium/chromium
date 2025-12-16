// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FilterDialogElement} from './filter_dialog.js';

export function getHtml(this: FilterDialogElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<dialog id="dialog" @close="${this.onClose}"
    @cancel="${this.onCancel}" @pointerdown="${this.onPointerDown}">
  <slot></slot>
</dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
