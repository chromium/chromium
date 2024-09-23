// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsNewBadgeElement} from './auto_tab_groups_new_badge.js';

export function getHtml(this: AutoTabGroupsNewBadgeElement) {
  return html`
<!--_html_template_start_-->
<div class="row">
  <div class="text">$i18n{newTabs}</div>
  <div class="divider"></div>
</div>
<!--_html_template_end_-->`;
}
