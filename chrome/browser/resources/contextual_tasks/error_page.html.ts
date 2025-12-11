// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksErrorPageElement} from './error_page.js';

// clang-format off
export function getHtml(this: ContextualTasksErrorPageElement) {
  return html`<!--_html_template_start_-->
  <div id="errorPage">
    <div id="errorIcon">
      <span id="protectedIcon"></span>
    </div>
    <div id="errorTopLine">
      $i18n{protectedErrorPageTopLine}
    </div>
    <div id="errorBottomLine">
      $i18n{protectedErrorPageBottomLine}
    </div>
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on
