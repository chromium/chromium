// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ZeroStateOverlayElement} from './zero_state_overlay.js';

export function getHtml(this: ZeroStateOverlayElement) {
  // clang-format off
  return html`<!--_html-template_start_-->
    <div id="pointerEventBlocker"> </div>
    <div id="overlay">
      <slot name="composebox"></slot>
    </div>
    <div id="opaqueOverlay"> </div>
    <!--TODO(crbug.com/461911563): Add dynamic suggestions-->
  <!--_html_template_end_-->`;
  // clang-format on
}
