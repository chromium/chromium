// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UpdaterAppElement} from './app.js';

export function getHtml(this: UpdaterAppElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<h2>Status</h2>
<updater-state></updater-state>
<h2>Event History</h1>
<event-list .messages="${this.messages}"></event-list>
<!--_html_template_end_-->`;
  // clang-format on
}
