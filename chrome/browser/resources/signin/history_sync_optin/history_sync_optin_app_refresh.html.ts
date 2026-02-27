// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistorySyncOptinAppRefreshElement} from './history_sync_optin_app_refresh.js';
import {LaunchContext} from './history_sync_optin.mojom-webui.js';

export function getHtml(this: HistorySyncOptinAppRefreshElement) {
  // clang-format off
  return html`
  <!--_html_template_start_-->
  <div id="contentContainer">
    <h1 class="title">$i18n{historySyncOptInTitle}</h1>
    <div id="illustrationContainer">
      <div id="imageContainer">
        <img class="dialog-illustration" alt="">
        <img id="avatar" alt="" src="${this.accountImageSrc_}">
      </div>
    </div>
    <div id="textContainer">
      <h2 id="subtitle">$i18n{historySyncOptInSubtitle}</h2>
      ${this.isLaunchContext_(LaunchContext.kWindow) ? html`
        <div id="description">$i18n{historySyncOptInDescription}</div>
      ` : ''}
    </div>
    <div id="buttonRow">
      <cr-button id="rejectButton" class="${this.getRejectButtonClass_()}"
          aria-label="$i18n{historySyncOptInRejectButtonLabel}"
          @click="${this.onRejectClick_}" ?disabled="${this.buttonsDisabled_}">
        $i18n{historySyncOptInRejectButtonLabel}
      </cr-button>
      <cr-button id="acceptButton" class="${this.getAcceptButtonClass_()}"
          aria-label="$i18n{historySyncOptInAcceptButtonLabel}"
          @click="${this.onAcceptClick_}" ?disabled="${this.buttonsDisabled_}">
        $i18n{historySyncOptInAcceptButtonLabel}
      </cr-button>
    </div>
  </div>
  ${this.isLaunchContext_(LaunchContext.kModal) ? html`
    <div id="modalDescription">$i18n{historySyncOptInDescription}</div>
  ` : ''}
  <!--_html_template_end_-->`;
}
