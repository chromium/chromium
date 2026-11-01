// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AccountMenuElement} from './account_menu.js';

export function getHtml(this: AccountMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-action-menu id="menu">
    </cr-action-menu>
  <!--_html_template_end_-->`;
  // clang-format on
}
