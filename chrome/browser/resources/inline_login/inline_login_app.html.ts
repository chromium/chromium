// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {InlineLoginAppElement} from './inline_login_app.js';

export function getHtml(this: InlineLoginAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="spinner" class="spinner" ?hidden="${!this.isSpinnerActive_()}">
</div>
<webview id="signinFrame" name="signin-frame" class="signin-frame"
    ?hidden="${this.isSpinnerActive_()}" allowscaling>
</webview>
<!--_html_template_end_-->`;
  // clang-format on
}
