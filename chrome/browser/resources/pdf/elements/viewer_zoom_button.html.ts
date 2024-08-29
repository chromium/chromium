// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerZoomButtonElement} from './viewer_zoom_button.js';

export function getHtml(this: ViewerZoomButtonElement) {
  return html`
<cr-icon-button .ironIcon="${this.computeVisibleIcon_()}"
    @click="${this.fireClick_}"
    aria-label="${this.computeVisibleTooltip_()}"
    title="${this.computeVisibleTooltip_()}"
    ?disabled="${this.disabled}">
</cr-icon-button>`;
}
