// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IndigoMotionOverlayElement} from './motion_overlay.js';

export function getHtml(this: IndigoMotionOverlayElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="blurLayer"></div>
<div id="swipeEllipse1"></div>
<div id="swipeEllipse2"></div>
<div id="loadingCircleDark"></div>
<div id="loadingCircleLight"></div>
<indigo-text-overlay id="textOverlay"></indigo-text-overlay>
<!--_html_template_end_-->`;
  // clang-format on
}
