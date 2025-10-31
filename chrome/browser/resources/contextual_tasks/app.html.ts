// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksAppElement} from './app.js';

// clang-format off
export function getHtml(this: ContextualTasksAppElement) {
  return html`<!--_html_template_start_-->
  <top-toolbar @signin-click="${this.removeGsc_}"></top-toolbar>
  <webview id="threadFrame" src="${this.threadUrl_}"></webview>
  <div id="composeboxContainer">
    <cr-composebox id="composebox">
    </cr-composebox>
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on
