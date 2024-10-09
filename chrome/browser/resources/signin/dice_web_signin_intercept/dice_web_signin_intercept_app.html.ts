// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {DiceWebSigninInterceptAppElement} from './dice_web_signin_intercept_app.js';

export function getHtml(this: DiceWebSigninInterceptAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div role="dialog" id="interceptDialog" aria-labelledby="title"
     aria-describedby="contents">
  ${this.interceptionParameters_.useV2Design ? html`
    <div id="headerV2">
      <svg>
        <use href="images/split_header.svg#EXPORT_primary"
            id="headerImagePrimary" >
        </use>
        <use href="images/split_header.svg#EXPORT_intercepted"
            id="headerImageIntercepted">
        </use>
      </svg>
      <div class="avatar-container-v2" id="avatarPrimary">
        <img class="avatar" alt=""
            src="${this.interceptionParameters_.primaryAccount.pictureUrl}">
        <div class="work-badge"
            ?hidden="${!this.interceptionParameters_.primaryAccount.avatarBadge.length}">
          <cr-icon class="icon"
              icon="${this.interceptionParameters_.primaryAccount.avatarBadge}">
          </cr-icon>
        </div>
      </div>
      <div class="avatar-container-v2" id="avatarIntercepted">
        <img class="avatar" alt=""
            src="${this.interceptionParameters_.interceptedAccount.pictureUrl}">
        <div class="work-badge"
            ?hidden="${!this.interceptionParameters_.interceptedAccount.avatarBadge.length}">
          <cr-icon class="icon"
              icon="${this.interceptionParameters_.interceptedAccount.avatarBadge}">
          </cr-icon>
        </div>
      </div>
    </div>
  ` : html`
    <div id="header">
      <div id="headerText">${this.interceptionParameters_.headerText}</div>
      <div id="avatarContainer">
        <img class="avatar" alt=""
            src="${this.interceptionParameters_.interceptedAccount.pictureUrl}">
        <div class="work-badge" id="badge"
            ?hidden="${!this.interceptionParameters_.interceptedAccount.avatarBadge.length}">
          <cr-icon class="icon"
              icon="${this.interceptionParameters_.interceptedAccount.avatarBadge}">
          </cr-icon>
        </div>
      </div>
    </div>
  `}

  <div id="body">
    <div id="title">${this.interceptionParameters_.bodyTitle}</div>
    <div id="contents">${this.interceptionParameters_.bodyText}</div>
    ${this.interceptionParameters_.showManagedDisclaimer ? html`
      <div id="managedDisclaimer">
        <div id="managedDisclaimerIcon">
          <cr-icon class="icon" icon="cr:domain"></cr-icon>
        </div>
        <div id="managedDisclaimerText"
            .innerHTML="${this.sanitizeInnerHtml_(
                this.interceptionParameters_.managedDisclaimerText)}">
        </div>
      </div>
    ` : ''}
  </div>

  <div id="actionRow">
    ${this.acceptButtonClicked_ ? html`<div class="spinner"></div>` : ''}
    <div class="action-container">
      <cr-button id="acceptButton" class="action-button" autofocus
          @click="${this.onAccept_}" ?disabled="${this.acceptButtonClicked_}">
        ${this.interceptionParameters_.confirmButtonLabel}
      </cr-button>
      <cr-button id="cancelButton" @click="${this.onCancel_}"
          ?disabled="${this.acceptButtonClicked_}">
        ${this.interceptionParameters_.cancelButtonLabel}
      </cr-button>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
