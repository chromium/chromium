// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToolbarAppElement} from './app.js';

export function getHtml(this: ToolbarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.isReloadButtonEnabled_ ?
  html`<reload-button-app id="reload"></reload-button-app>` : ''}
${this.isSplitTabsButtonEnabled_ ?
  html`<split-tabs-button-app></split-tabs-button-app>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
