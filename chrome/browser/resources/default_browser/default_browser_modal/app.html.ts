// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DefaultBrowserModalAppElement} from './app.js';

export function getHtml(this: DefaultBrowserModalAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <div id="top-section">
    ${this.useSettingsIllustration ? html`
    <img id="icon" src="chrome_logo.svg" alt="">
    ` : html`
    <div id="icon-container">
      <img id="header-background" alt="">
      <img id="icon" src="chrome_logo.svg" alt="">
    </div>
    `}

    <div id="text-content">
      <div id="title">$i18n{title}</div>
      <div id="body-text">$i18n{bodyText}</div>
    </div>

    ${this.useSettingsIllustration ? html`
    <div id="illustration">
      <img src="settings_illustration.svg" alt="">
    </div>
    ` : ''}
  </div>
  <div id="bottom-section">
    <div id="button-row">
      <cr-button id="cancel-button" @click="${this.onCancelClick_}">
        $i18n{cancelButton}
      </cr-button>
      <cr-button id="confirm-button" class="action-button"
          @click="${this.onConfirmClick_}">
        $i18n{confirmButton}
      </cr-button>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
