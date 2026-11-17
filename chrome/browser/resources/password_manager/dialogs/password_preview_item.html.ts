// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordPreviewItemElement} from './password_preview_item.js';

export function getHtml(this: PasswordPreviewItemElement) {
  return html`<!--_html_template_start_-->
<div class="flex-centered">
  <cr-checkbox id="checkbox" ?checked="${this.checked}"
      @checked-changed="${this.onCheckedChanged_}">
    ${this.url}, ${this.username}
  </cr-checkbox>
  <div id="container" class="${this.getElementClass_()}">
    ${this.url ? html`
      <site-favicon domain="${this.url}" aria-hidden="true"></site-favicon>
      <div id="website" class="text-elide">${this.url}</div>
    ` : ''}

    <div id="username" class="text-elide">${this.username}</div>
    <div id="password-wrapper">
      <input id="password" readonly class="text-elide password-input"
          type="${this.getPasswordInputType()}"
          ?disabled="${!this.isPasswordVisible}"
          .value="${this.getPasswordValue_()}">
      <cr-icon-button id="showPasswordButton"
          title="${this.getShowHideButtonLabel()}"
          class="${this.getShowHideButtonIconClass()}"
          @click="${this.onShowHidePasswordButtonClick}"
          aria-label="${this.getShowHidePasswordButtonA11yLabel_()}">
      </cr-icon-button>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
}
