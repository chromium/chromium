// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExceptionEntryElement} from './exception_entry.js';

export function getHtml(this: ExceptionEntryElement) {
  return html`<!--_html_template_start_-->
<div class="list-item">
  <div class="start text-elide">${this.entry.site}</div>
  ${this.entry.managed ? html`
    <cr-policy-pref-indicator .pref="${this.exceptionsManagedPref_}"
        @mouseenter="${this.onShowTooltipMouseenter_}"
        @focus="${this.onShowTooltipFocus_}">
    </cr-policy-pref-indicator>
  ` : html`
    ${this.settingsRefresh2026Enabled_ ? html`
      <cr-icon-button id="editButton" class="icon-edit" title="$i18n{edit}"
          aria-label="$i18n{edit}" @click="${this.onEditClick_}">
      </cr-icon-button>
      <cr-icon-button id="deleteButton" iron-icon="settings20:delete"
          title="$i18n{siteSettingsActionReset}"
          aria-label="$i18n{siteSettingsActionReset}"
          @click="${this.onDeleteClick_}">
      </cr-icon-button>
    ` : html`
      <cr-icon-button class="icon-more-vert" title="$i18n{moreActions}"
          @click="${this.onMenuClick_}" aria-label="$i18n{moreActions}">
      </cr-icon-button>
    `}
  `}
</div>
<!--_html_template_end_-->`;
}
