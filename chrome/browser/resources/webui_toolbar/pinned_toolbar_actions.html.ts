// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PinnedToolbarActionsElement} from './pinned_toolbar_actions.js';

export function getHtml(this: PinnedToolbarActionsElement) {
  return html`<!--_html_template_start_-->
${this.state.map(state => html`
  <pinned-toolbar-action .state="${state}">
  </pinned-toolbar-action>
`)}
<!--_html_template_end_-->`;
}
