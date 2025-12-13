// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistorySyncOptinAppElement} from './history_sync_optin_app.js';
import {LaunchContext} from './history_sync_optin.mojom-webui.js';

export function getHtml(this: HistorySyncOptinAppElement) {
  // clang-format off
  return html`
  <!--_html_template_start_-->
  <div id="contentContainer">
    <div id="illustrationContainer">
      ${this.isLaunchContext_(LaunchContext.kWindow) ? html`
        <img class="window-left-banner-illustration" alt="">
        <img class="window-right-banner-illustration" alt="">
      ` : ''}
      <div id="imageContainer">
        <img class="dialog-illustration" alt="">
        <img id="avatar" alt="" src="${this.accountImageSrc_}">
      </div>
    </div>
    <div id="textContainer">
      <h1 class="title">$i18n{historySyncOptInTitle}</h1>
      <div id="subtitle">$i18n{historySyncOptInSubtitle}</div>
      ${this.isLaunchContext_(LaunchContext.kWindow) ? html`
        <div id="description">$i18n{historySyncOptInDescription}</div>
      ` : ''}
    </div>
    <div id="buttonRow">
      <cr-button id="rejectButton" class="${this.getRejectButtonClass_()}"
          @click="${this.onReject_}" ?disabled="${this.buttonsDisabled_}">
        $i18n{historySyncOptInRejectButtonLabel}
      </cr-button>
      <cr-button id="acceptButton" class="${this.getAcceptButtonClass_()}"
          @click="${this.onAccept_}" ?disabled="${this.buttonsDisabled_}">
        $i18n{historySyncOptInAcceptButtonLabel}
      </cr-button>
    </div>
  </div>
  ${this.isLaunchContext_(LaunchContext.kModal) ? html`
    <div id="modalDescription">$i18n{historySyncOptInDescription}</div>
  ` : ''}
  <!--_html_template_end_-->`;
}
