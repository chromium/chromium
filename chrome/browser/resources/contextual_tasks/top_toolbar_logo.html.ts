// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

// clang-format off
export function getHtml() {
  return html`<!--_html_template_start_-->
<if expr="_google_chrome">
  <img src="chrome://resources/cr_components/searchbox/icons/google_g_gradient.svg"
      class="top-toolbar-logo"
      alt=""
      aria-hidden="true">
</if>
<if expr="not _google_chrome">
  <img class="top-toolbar-logo chrome-logo-light"
      src="chrome://resources/cr_components/searchbox/icons/chrome_product.svg"
      alt=""
      aria-hidden="true">
  <img class="top-toolbar-logo chrome-logo-dark"
      src="chrome://resources/images/chrome_logo_dark.svg"
      alt=""
      aria-hidden="true">
</if>
<!--_html_template_end_-->`;
}
// clang-format on
