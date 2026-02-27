// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SigninErrorAppElement} from './signin_error_app.js';

export function getHtml(this: SigninErrorAppElement) {
  return html`<!--_html_template_start_-->
<div class="container">
  <div class="top-title-bar">$i18n{signinErrorTitle}</div>
  <div id="normal-error-message" class="details"
      ?hidden="${this.hideNormalError_}">
    <p>$i18nRaw{signinErrorMessage}</p>
    <a id="learnMoreLink" href="#" @click="${this.onLearnMoreClick_}">
      $i18nRaw{signinErrorLearnMore}
    </a>
  </div>
  <div class="action-container">
    <cr-button class="action-button" id="switchButton"
        ?hidden="${this.switchButtonUnavailable_}"
        @click="${this.onSwitchToExistingProfileClick_}">
      $i18n{signinErrorSwitchLabel}
    </cr-button>
    <cr-button id="closeButton" ?hidden="${this.switchButtonUnavailable_}"
        @click="${this.onConfirmClick_}" autofocus>
      $i18n{signinErrorCloseLabel}
    </cr-button>
    <cr-button id="confirmButton" ?hidden="${!this.switchButtonUnavailable_}"
        @click="${this.onConfirmClick_}">
      $i18n{signinErrorOkLabel}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
