// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SignoutConfirmationAppElement} from './signout_confirmation_app.js';

export function getHtml(this: SignoutConfirmationAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="signoutConfirmationDialog">
  <div id="header">
    <h1 id="title">${this.data_.dialogTitle}</h1>
    <div id="subtitle">
      <p>${this.data_.dialogSubtitle}</p>
      <p id="extensionsAdditionalText"
          ?hidden="${!this.showExtensionsAdditionalText_()}">
        $i18n{unsyncedDataWithAccountExtensions}
      </p>
    </div>
  </div>
  ${this.showExtensionsSection_() ? html`
    <extensions-section .accountExtensions="${this.data_.accountExtensions}">
    </extensions-section>
  ` : ``}
  <div id="action-row">
    <cr-button id="acceptButton" class="action-button"
        @click="${this.onAcceptButtonClick_}" autofocus>
      ${this.data_.acceptButtonLabel}
    </cr-button>
    <cr-button id="cancelButton" class="tonal-button"
        @click="${this.onCancelButtonClick_}">
      ${this.data_.cancelButtonLabel}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
