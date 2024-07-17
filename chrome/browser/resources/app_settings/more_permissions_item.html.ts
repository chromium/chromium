// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MorePermissionsItemElement} from './more_permissions_item.js';

export function getHtml(this: MorePermissionsItemElement) {
  return html`<!--_html_template_start_-->
<div id="label" aria-hidden="true">${this.morePermissionsLabel}</div>
<div class="permission-row-controls">
  <cr-icon-button class="native-settings-icon icon-external" role="link"
      tabindex="0" aria-labelledby="label">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
}
