// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NuxGoogleAppsElement} from './nux_google_apps.js';

export function getHtml(this: NuxGoogleAppsElement) {
  return html`<!--_html_template_start_-->
<div class="apps-ask">
  <div class="chrome-logo" aria-hidden="true"></div>
  <h1 tabindex="-1">${this.subtitle}</h1>
  <div id="appChooser">
    <div class="slide-in">
      ${this.appList_.map((item, index) => html`
        <button ?active="${item.selected}"
            aria-pressed="${item.selected}"
            data-index="${index}" @click="${this.onAppClick_}"
            @pointerdown="${this.onAppPointerDown_}"
            @keyup="${this.onAppKeyUp_}" class="option">
          <div class="option-icon-shadow">
            <div class="${item.icon} option-icon"></div>
          </div>
          <div class="option-name">${item.name}</div>
          <cr-icon icon="cr:check"></cr-icon>
        </button>
      `)}
    </div>

    <div class="button-bar">
      <cr-button id="noThanksButton" @click="${this.onNoThanksClicked_}">
        $i18n{skip}
      </cr-button>
      <step-indicator .model="${this.indicatorModel}"></step-indicator>
      <cr-button class="action-button" ?disabled="${!this.hasAppsSelected_}"
          @click="${this.onNextClicked_}">
        $i18n{next}
        <cr-icon icon="cr:chevron-right"></cr-icon>
      </cr-button>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
}
