// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OverflowMenuElement} from './overflow_menu.js';

// clang-format off
export function getHtml(this: OverflowMenuElement) {
  return html`<!--_html_template_start_-->
    <cr-action-menu id="menu" @open-changed="${this.onOpenChanged_}">
      ${this.shouldShowNewThreadInMenu_() ? html`
        <button class="dropdown-item" id="newThreadButton" @click="${this.onNewThreadClick_}">
          <cr-icon
              icon="${this.webuiRoundedIconsEnabled_
      ? 'contextual_tasks:edit-square'
      : 'contextual_tasks:edit_square-old'}"></cr-icon>
          $i18n{newThreadTooltip}
        </button>
      ` : ''}
      ${this.shouldShowThreadHistoryInMenu_() ? html`
        <button class="dropdown-item" @click="${this.onThreadHistoryClick_}">
          <cr-icon
              icon="${this.webuiRoundedIconsEnabled_
      ? 'contextual_tasks:notes-spark'
      : 'contextual_tasks:notes_spark-old'}"></cr-icon>
          $i18n{threadHistoryTooltip}
        </button>
      ` : ''}
      ${this.shouldShowOpenInNewTabInMenu_() ? html`
        <button class="dropdown-item" id="openInNewTabButton"
            @click="${this.onOpenInNewTabClick_}"
            ?disabled="${!this.enableOpenInNewTabButton}">
          <cr-icon
              icon="${this.webuiRoundedIconsEnabled_
      ? 'contextual_tasks:open-in-full'
      : 'contextual_tasks:open_in_full_tab-old'}"></cr-icon>
          $i18n{openInNewTab}
        </button>
      ` : ''}
      ${this.shouldShowPinButton_() ? html`
        <button class="dropdown-item" id="pinButton" @click="${this.onPinClick_}">
          <cr-icon icon="${this.isPinned
      ? (this.webuiRoundedIconsEnabled_ ? 'contextual_tasks:keep-off'
        : 'contextual_tasks:keep_off-old')
      : (this.webuiRoundedIconsEnabled_ ? 'contextual_tasks:keep'
        : 'contextual_tasks:keep-old')}"></cr-icon>
          ${this.getPinButtonTooltip_()}
        </button>
      ` : ''}
      ${this.shouldShowMenuHeaderDivider_() ? html`
        <div class="dropdown-divider"></div>
      ` : ''}
      <button class="dropdown-item" @click="${this.onMyActivityClick_}">
<if expr="_google_chrome">
        <cr-icon
            icon="${this.webuiRoundedIconsEnabled_
      ? 'contextual_tasks:google'
      : 'contextual_tasks:g_logo-old'}"></cr-icon>
</if>
<if expr="not _google_chrome">
        <cr-icon icon="cr:history"></cr-icon>
</if>
        $i18n{myActivity}
      </button>
      ${this.contextualTasksEnableSpatialModelToolbarLayout ? html`
        <button class="dropdown-item" id="helpButton" @click="${this.onHelpClick_}">
          <cr-icon icon="cr:help"></cr-icon>
          $i18n{help}
        </button>
      ` : ''}
      ${this.isUserFeedbackAllowed ? html`
        <button class="dropdown-item" @click="${this.onFeedbackClick_}">
          <cr-icon
              icon="${this.webuiRoundedIconsEnabled_
      ? 'contextual_tasks:feedback'
      : 'contextual_tasks:feedback-old'}"></cr-icon>
          $i18n{feedback}
        </button>
      ` : ''}
    </cr-action-menu>
<!--_html_template_end_-->`;
}
// clang-format on
