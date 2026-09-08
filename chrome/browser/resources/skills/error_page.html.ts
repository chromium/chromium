// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '/strings.m.js';

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {ErrorPageElement} from './error_page.js';

export function getHtml(this: ErrorPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container"
    role="${this.dialog ? 'dialog' : nothing}"
    aria-modal="${this.dialog ? 'true' : nothing}"
    aria-labelledby="${this.dialog ? 'errorTitle' : nothing}">
  <div id="mainContent">
    <div id="header">
      ${this.isGlicDisabled_() || this.isSkillsDisabled_() ||
        this.isRemoteAuthorityUnreachable_() ? html`
        <cr-icon icon="cr:error"></cr-icon>` : ''}
      <h1 id="errorTitle" class="headline">${this.errorTitle()}</h1>
    </div>
    <p class="body-text">${this.errorDescription()}</p>
    ${!this.dialog && this.isSkillsDisabled_() ? html`
      <cr-button @click="${this.onGoToSettingsClick_}">
        $i18n{goToSettings}
      </cr-button>` : ''}
  </div>
  ${this.dialog ? html`
    <div class="dialog-actions-row">
      <cr-button id="cancelButton" class="cancel-button"
          @click="${this.onCancelClick_}">
        $i18n{cancel}
      </cr-button>
      ${this.isGlicDisabled_() ? html`
        <cr-button id="signInButton" class="action-button"
            @click="${this.onSignInClick_}">
          $i18n{signInToChrome}
        </cr-button>` : ''}
      ${this.isSkillsDisabled_() ? html`
        <cr-button id="settingsButton" class="action-button"
            @click="${this.onGoToSettingsClick_}">
          $i18n{goToSettings}
        </cr-button>` : ''}
    </div>` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
