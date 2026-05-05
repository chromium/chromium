// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OverflowMenuElement} from './overflow_menu.js';

// clang-format off
export function getHtml(this: OverflowMenuElement) {
  return html`<!--_html_template_start_-->
    <cr-action-menu id="menu">
      ${this.isSmallDeviceFormFactor ? html`
        <button class="dropdown-item" @click="${this.onThreadHistoryClick_}">
          <cr-icon icon="contextual_tasks:notes_spark"></cr-icon>
          $i18n{threadHistoryTooltip}
        </button>
      ` : html`
        <button class="dropdown-item"
            @click="${this.onOpenInNewTabClick_}"
            ?disabled="${!this.enableOpenInNewTabButton}">
          <cr-icon icon="contextual_tasks:open_in_full_tab"></cr-icon>
          $i18n{openInNewTab}
        </button>
        <div class="dropdown-divider"></div>
      `}
      <button class="dropdown-item" @click="${this.onMyActivityClick_}">
<if expr="_google_chrome">
        <cr-icon icon="contextual_tasks:g_logo"></cr-icon>
</if>
<if expr="not _google_chrome">
        <cr-icon icon="cr:history"></cr-icon>
</if>
        $i18n{myActivity}
      </button>
      <button class="dropdown-item" @click="${this.onFeedbackClick_}">
        <cr-icon icon="contextual_tasks:feedback"></cr-icon>
        $i18n{feedback}
      </button>
    </cr-action-menu>
<!--_html_template_end_-->`;
}
// clang-format on
