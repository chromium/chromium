// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShoppingInsightsHistoryGraphElement} from './history_graph.js';

export function getHtml(this: ShoppingInsightsHistoryGraphElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="historyGraph" tabindex="0" aria-live="polite"></div>
<!--_html_template_end_-->`;
  // clang-format on
}
