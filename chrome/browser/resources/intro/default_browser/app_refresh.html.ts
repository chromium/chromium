// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppRefreshElement} from './app_refresh.js';

export function getHtml(this: AppRefreshElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="header"></div>
<div id="content-div">

  <div class="content-child" id="showcase-illustration">
    <img id="illustration" alt="">
  </div>

  <div class="content-child" id="showcase-description">
    <div id="stepper-placeholder">
      <img id="keep-icon" src="images/keep_icon.svg" alt="">
    </div>
    <div id="showcase-text">
      <h1 class="title">$i18n{refreshDefaultBrowserTitle}</h1>
      <p class="subtitle">$i18n{refreshDefaultBrowserSubtitle}</p>
      <div id="button-container">
        <cr-button id="confirm-button" class="action-button"
            @click="${this.onConfirmDefaultBrowserClick_}"
            ?disabled="${this.anyButtonClicked_}">
          $i18n{refreshDefaultBrowserSetAsDefault}
        </cr-button>
        <cr-button id="skip-button" @click="${this.onSkipDefaultBrowserClick_}"
            ?disabled="${this.anyButtonClicked_}">
          $i18n{refreshDefaultBrowserNoThanks}
        </cr-button>
      </div>
    </div>
  </div>

</div>
<!--_html_template_end_-->`;
  // clang-format on
}
