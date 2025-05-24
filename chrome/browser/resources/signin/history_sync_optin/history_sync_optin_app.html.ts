// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistorySyncOptinAppElement} from './history_sync_optin_app.js';

export function getHtml(this: HistorySyncOptinAppElement) {
  return html`
  <!--_html_template_start_-->
  <div id="contentContainer">
    <div id="imageContainer">
      <img class="dialog-illustration" alt="">
      <img id="avatar" alt="" src="${this.accountImageSrc_}">
    </div>
    <div id="textContainer">
      <h1 class="title">$i18n{historySyncOptInTitle}</h1>
      <div id="subtitle">$i18n{historySyncOptInSubtitle}</div>
    </div>
    <div id="buttonRow">
      <cr-button id="rejectButton" class="tonal-button"
          @click="${this.onReject_}">
        $i18n{historySyncOptInRejectButtonLabel}
      </cr-button>
      <cr-button id="acceptButton" class="action-button"
          @click="${this.onAccept_}">
        $i18n{historySyncOptInAcceptButtonLabel}
      </cr-button>
    </div>
  </div>
  <div id="modalDescription">$i18n{historySyncOptInDescription}</div>
  <!--_html_template_end_-->`;
}
