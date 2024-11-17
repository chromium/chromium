// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SigninReauthAppElement} from './signin_reauth_app.js';

export function getHtml(this: SigninReauthAppElement) {
  return html`<!--_html_template_start_-->
<!--
  Use the 'consent-description' attribute to annotate all the UI elements
  that are part of the text the user reads before consenting to use passwords
  from their account. Similarly, use 'consent-confirmation' on the UI element on
  which user clicks to indicate consent.
-->

<div id="illustrationContainer">
  <div id="illustration"></div>
  <img src="${this.accountImageSrc_}" alt="">
</div>
<div id="contentContainer">
  <h1 id="signinReauthTitle" consent-description>
    $i18n{signinReauthTitle}
  </h1>
  <div class="message-container">
    <div consent-description>
      $i18n{signinReauthDesc}
    </div>
  </div>
</div>
<div class="action-container">
  ${this.confirmButtonHidden_ ? html`<div class="spinner"></div>` : ''}
  <cr-button id="confirmButton" class="action-button"
      @click="${this.onConfirm_}" ?hidden="${this.confirmButtonHidden_}"
      consent-confirmation>
    $i18n{signinReauthConfirmLabel}
  </cr-button>
  <cr-button id="cancelButton" @click="${this.onCancel_}"
      ?hidden="${this.cancelButtonHidden_}">
    $i18n{signinReauthCloseLabel}
  </cr-button>
</div>
<!--_html_template_end_-->`;
}
