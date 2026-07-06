// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageActionIconsElement} from './page_action_icons.js';

export function getHtml(this: PageActionIconsElement) {
  return html`<!--_html_template_start_-->
${this.pageActionStates.map(item => html`
  <page-action-icon .state="${item}">
  </page-action-icon>
`)}
<!--_html_template_end_-->`;
}
