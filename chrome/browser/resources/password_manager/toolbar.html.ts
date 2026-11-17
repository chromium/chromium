// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordManagerToolbarElement} from './toolbar.js';

export function getHtml(this: PasswordManagerToolbarElement) {
  return html`<!--_html_template_start_-->
<cr-toolbar id="mainToolbar" @keydown="${this.onKeydown_}"
    page-name="${this.pageName}" clear-label="$i18n{clearSearch}"
    search-prompt="$i18n{searchPrompt}" menu-label="$i18n{menuButtonLabel}"
    autofocus @search-changed="${this.onSearchChanged_}" role="banner"
    ?show-menu="${this.narrow}" ?narrow="${this.narrow}"
    narrow-threshold="1300">
  <picture slot="product-logo">
    <img id="product-logo"
        srcset="chrome://password-manager/images/password_manager_logo.svg"
        role="presentation">
  </picture>
  <cr-icon-button id="helpButton" iron-icon="cr:help" title="$i18n{help}"
      @click="${this.onHelpClick_}" suppress-rtl-flip>
  </cr-icon-button>
</cr-toolbar>
<!--_html_template_end_-->`;
}
