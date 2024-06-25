// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabOrganizationNewBadgeElement} from './tab_organization_new_badge.js';

export function getHtml(this: TabOrganizationNewBadgeElement) {
  return html`
<!--_html_template_start_-->
<div class="row">
  <div class="text">$i18n{newTabs}</div>
  <div class="divider"></div>
</div>
<!--_html_template_end_-->`;
}
