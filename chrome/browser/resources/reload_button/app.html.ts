// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReloadButtonAppElement} from './app.js';

export function getHtml(this: ReloadButtonAppElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button class="${this.isLoading_ ? 'icon-clear' : 'icon-refresh'}"
    title="${this.tooltip_}"
    @pointerdown="${this.onReloadButtonPointerDown_}"
    @pointerup="${this.onReloadButtonPointerUp_}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
