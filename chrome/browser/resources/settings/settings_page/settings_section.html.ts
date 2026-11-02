/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSectionElement} from './settings_section.js';

export function getHtml(this: SettingsSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="header" ?hidden="${!this.pageTitle && !this.showSendFeedbackButton}">
  <h2 id="title" class="title" tabindex="-1"
      aria-hidden="${this.getTitleHiddenStatus_() || nothing}">
    ${this.pageTitle}
  </h2>
  ${this.showSendFeedbackButton ? html`
    <cr-icon-button id="feedback" iron-icon="settings:feedback"
        aria-labelledby="title" suppress-rtl-flip
        aria-roledescription="$i18n{sendFeedbackButton}"
        @click="${this.onSendFeedbackClick_}">
    </cr-icon-button>
  ` : ''}
</div>
<div id="card">
  <slot></slot>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
