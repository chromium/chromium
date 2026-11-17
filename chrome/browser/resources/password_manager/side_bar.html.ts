// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordManagerSideBarElement} from './side_bar.js';

export function getHtml(this: PasswordManagerSideBarElement) {
  return html`<!--_html_template_start_-->
<div role="navigation">
  <cr-menu-selector id="menu" attr-for-selected="path"
      selected-attribute="selected"
      @iron-activate="${this.onSelectorIronActivate_}"
      .selected="${this.getSelectedPage_()}">
    <a id="passwords" role="menuitem" class="cr-nav-menu-item" path="passwords"
        href="/passwords" @click="${this.onItemClick_}">
      <cr-icon icon="cr20:password-manager"></cr-icon>
      $i18n{passwords}
      <cr-ripple></cr-ripple>
    </a>
    <a id="checkup" role="menuitem" class="cr-nav-menu-item" path="checkup"
        href="/checkup" @click="${this.onItemClick_}">
      <cr-icon icon="${this.getInventoryIcon_()}"></cr-icon>
      <span>$i18n{checkup}</span>
      <div id="compromisedPasswords" ?hidden="${!this.compromisedPasswords_}">
        ${this.getCompromisedPasswordsBadge_()}
      </div>
      <cr-ripple></cr-ripple>
    </a>
    <a id="settings" role="menuitem" class="cr-nav-menu-item" path="settings"
        href="/settings" @click="${this.onItemClick_}">
      <cr-icon icon="${this.getSettingsIcon_()}"></cr-icon>
      $i18n{settings}
      <cr-ripple></cr-ripple>
    </a>
  </cr-menu-selector>
</div>
<!--_html_template_end_-->`;
}
