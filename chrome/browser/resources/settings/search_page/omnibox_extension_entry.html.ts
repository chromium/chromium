// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsOmniboxExtensionEntryElement} from './omnibox_extension_entry.js';

export function getHtml(this: SettingsOmniboxExtensionEntryElement) {
  return html`
<!--_html_template_start_-->
<div class="list-item" focus-row-container>
  <div class="name-column">
    <site-favicon .faviconUrl="${this.engine.iconURL || ''}"></site-favicon>
    <span>${this.engine.displayName}</span>
  </div>
  <div class="keyword-column">${this.engine.keyword}</div>
  <cr-icon-button class="icon-more-vert" @click="${this.onDotsClick_}"
      title="$i18n{moreActions}" focus-row-control focus-type="menu">
  </cr-icon-button>
  <cr-action-menu role-description="$i18n{menu}">
    <button class="dropdown-item" @click="${this.onManageClick_}"
        id="manage">
      $i18n{searchEnginesManageExtension}
    </button>
    <button class="dropdown-item" @click="${this.onDisableClick_}"
        id="disable">
      $i18n{disable}
    </button>
  </cr-action-menu>
</div>
<!--_html_template_end_-->`;
}
