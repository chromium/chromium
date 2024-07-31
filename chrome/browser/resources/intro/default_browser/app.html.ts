// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<img class="tangible-sync-style-left-banner" alt="">
<img class="tangible-sync-style-right-banner" alt="">

<div id="safe-zone" class="tangible-sync-style">
  <div id="content-area">
    <div id="illustration-container" role="img"
        aria-label="$i18n{defaultBrowserIllustrationAltText}">
      <img id="default-browser-frame" alt="">
      <img id="product-logo" src="images/product-logo.svg" alt="">
    </div>
    <h1 class="title">$i18n{defaultBrowserTitle}</h1>
    <p class="subtitle">$i18n{defaultBrowserSubtitle}</p>
  </div>
</div>

<div id="button-row" class="tangible-sync-style">
  <div id="button-container">
    <cr-button id="skip-button" @click="${this.onSkipDefaultBrowserClick_}"
        ?disabled="${this.areButtonsDisabled_()}">
      $i18n{defaultBrowserSkip}
    </cr-button>
    <cr-button id="confirmButton" class="action-button"
        @click="${this.onConfirmDefaultBrowserClick_}"
        ?disabled="${this.areButtonsDisabled_()}">
      $i18n{defaultBrowserSetAsDefault}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
