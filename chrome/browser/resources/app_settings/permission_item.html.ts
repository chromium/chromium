// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PermissionItemElement} from './permission_item.js';

export function getHtml(this: PermissionItemElement) {
  return html`<!--_html_template_start_-->
<!-- permission-item does not include any icon-set, so containing
  elements should import the icon-set needed for the specified |icon|. -->
${this.available_ ? html`
  <app-management-toggle-row
      id="toggle-row"
      icon="${this.icon}"
      label="${this.permissionLabel}"
      ?managed="${this.isManaged_()}"
      ?disabled="${this.isDisabled_()}"
      ?value="${this.getValue_()}"
      aria-description="Click to toggle ${this.permissionLabel} permissions."
      i18n-aria-descrirption="Label for toggle button to change ${this.permissionLabel} permissions.">
      <slot name="description" slot="description"></slot>
  </app-management-toggle-row>
` : ''}
<!--_html_template_end_-->`;
}
