// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AccessibilityAnnotatorInfoElement} from './accessibility_annotator_info.js';

export function getHtml(this: AccessibilityAnnotatorInfoElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <div class="account-info" ?hidden="${!this.email_}">
    <img id="avatar" src="${this.avatarUrl_}" alt="">
    <span id="email">${this.email_}</span>
  </div>
  <div class="canvasDiv">
    <div class="logo-container">
      <!-- TODO(crbug.com/488321731): Add illustration here. -->
    </div>
  </div>
  <h1 class="title">$i18n{accessibilityAnnotatorInfoTitle}</h1>
  <div class="description">
    <p>
      $i18n{accessibilityAnnotatorInfoDescription}
    </p>
    <div class="features-container">
      <div class="feature-item">
        <div class="feature-icon">
          <img src="keyboard.svg" alt="">
        </div>
        <div class="feature-text" id="triggerCard">
          ${this.i18n('accessibilityAnnotatorInfoCard1')
            .split('$1')
            .map((text, i, arr) => html`
              ${text}${
                i < arr.length - 1 ?
                  html`<span class="pill">
                    ${this.i18n('accessibilityAnnotatorTriggerText')}
                  </span>` :
                  ''
              }`)}
        </div>
      </div>
      <div class="feature-item">
        <div class="feature-icon">
          <div class="g-icon"></div>
        </div>
        <div class="feature-text">
          $i18n{accessibilityAnnotatorInfoCard2}
        </div>
      </div>
    </div>
        <p class="footer-text" .innerHTML=
            "${this.i18nAdvanced('accessibilityAnnotatorInfoLearnMore')}">
    </p>
  </div>

  <div class="actions">
    <cr-button id="manageSettings" class="tonal-button"
      @click="${this.onManageSettingsClick_}">
      $i18n{accessibilityAnnotatorInfoSecondaryButton}
    </cr-button>
    <cr-button id="gotIt" class="action-button" @click="${this.onGotItClick_}">
      $i18n{accessibilityAnnotatorInfoPrimaryButton}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
