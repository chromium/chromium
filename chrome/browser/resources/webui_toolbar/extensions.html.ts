// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsElement} from './extensions.js';

export function getHtml(this: ExtensionsElement) {
  return html`<!--_html_template_start_-->
${this.state.map(state => html`
  <webui-toolbar-extension .state="${state}">
  </webui-toolbar-extension>
`)}
${this.state.length > 0 ? html`<toolbar-divider></toolbar-divider>` : ''}
<!--_html_template_end_-->`;
}
