// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AccountMenuElement} from './account_menu.js';

export function getHtml(this: AccountMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-action-menu id="menu">
      <div id="accountInfo">${this.getAccountLabel_()}</div>
      <button id="manageAccountButton" class="dropdown-item"
          @click="${this.onManageAccountClick_}">
        $i18n{accountMenuManageAccount}
      </button>
      <button id="signOutButton" class="dropdown-item"
          @click="${this.onSignOutClick_}">
        $i18n{accountMenuSignOut}
      </button>
    </cr-action-menu>
  <!--_html_template_end_-->`;
  // clang-format on
}
