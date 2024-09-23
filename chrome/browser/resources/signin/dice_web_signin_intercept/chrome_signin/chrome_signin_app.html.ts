// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ChromeSigninAppElement} from './chrome_signin_app.js';

export function getHtml(this: ChromeSigninAppElement) {
  return html`<!--_html_template_start_-->
<div role="dialog" id="interceptDialog" aria-labelledby="title"
    aria-describedby="contents">

  <div id="header-container">
    <img id="product-logo"
      srcset="chrome://theme/current-channel-logo@1x 1x,
              chrome://theme/current-channel-logo@2x 2x"
      role="presentation">

    <h1 id="title">${this.interceptionParameters_.title}</h1>
    <div id="subtitle">${this.interceptionParameters_.subtitle}</div>
  </div>

  <div id="contents-container">
    <div id="contents">

      <div class="account-icon-container" id="accountIconContainer">
        <img class="account-icon" alt=""
            src="${this.interceptionParameters_.pictureUrl}">
        <div class="managed-user-badge"
            ?hidden="${!this.interceptionParameters_.managedUserBadge.length}">
          <cr-icon class="icon"
              icon="${this.interceptionParameters_.managedUserBadge}"></cr-icon>
        </div>
      </div>

      <div id="name">${this.interceptionParameters_.fullName}</div>
      <div id="email">${this.interceptionParameters_.email}</div>

      <div id="button-container">
        <cr-button id="accept-button" class="action-button"
            aria-label="${this.getAcceptButtonAriaLabel_()}"
            @click="${this.onAccept_}">
          <div id="acceppt-button-content">
            ${this.i18n('chromeSigninAcceptText',
               this.interceptionParameters_.givenName)}
          </div>
        </cr-button>
        <cr-button id="cancel-button" @click="${this.onCancel_}">
          $i18n{chromeSigninDeclineText}
        </cr-button>
      </div>

    </div>
  </div>

</div>
<!--_html_template_end_-->`;
}
