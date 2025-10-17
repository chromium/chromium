// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksAppElement} from './app.js';

// clang-format off
export function getHtml(this: ContextualTasksAppElement) {
  return html`<!--_html_template_start_-->
  <div id="toolbar">
    Contextual Tasks UI
  </div>
  <!-- TODO(452978117): Switch back to webview tag once it is supported. -->
  <iframe id="threadFrame" src="${this.threadUrl_}"></iframe>
  <div id="composeboxContainer">
    <ntp-composebox id="composebox">
    </ntp-composebox>
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on
