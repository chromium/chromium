// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsStartupUrlEntryElement} from './startup_url_entry.js';

export function getHtml(this: SettingsStartupUrlEntryElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="list-item">
  <site-favicon .url="${this.model.url}"></site-favicon>
  <div class="middle hide-overflow">
    <div class="text-elide">${this.model.title}</div>
    <div class="text-elide secondary">${this.model.url}</div>
  </div>
  ${this.editable ? html`
    <cr-icon-button class="icon-more-vert" id="dots"
        @click="${this.onDotsClick_}" title="${this.getMoreActionsTitle_()}">
    </cr-icon-button>
    <cr-lazy-render-lit id="menu" .template="${() => html`
      <cr-action-menu role-description="$i18n{menu}">
        <button class="dropdown-item" @click="${this.onEditClick_}">
          $i18n{edit}
        </button>
        <button class="dropdown-item" id="remove"
            @click="${this.onRemoveClick_}">
          $i18n{onStartupRemove}
        </button>
      </cr-action-menu>`}">
    </cr-lazy-render-lit>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
