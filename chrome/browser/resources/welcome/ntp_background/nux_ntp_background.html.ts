// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NuxNtpBackgroundElement} from './nux_ntp_background.js';

export function getHtml(this: NuxNtpBackgroundElement) {
  return html`<!--_html_template_start_-->
<div id="backgroundPreview"
    @transitionend="${this.onBackgroundPreviewTransitionEnd_}">
</div>

<div class="content">
  <div class="ntp-background-logo" aria-hidden="true"></div>
  <h1 tabindex="-1">${this.subtitle}</h1>

  <div class="ntp-backgrounds-grid slide-in">
    ${this.backgrounds_.map((item, index) => html`
      <button
          ?active="${this.isSelectedBackground_(item)}"
          class="option" data-index="${index}"
          @click="${this.onBackgroundClick_}"
          @keyup="${this.onBackgroundKeyUp_}"
          @pointerdown="${this.onBackgroundPointerDown_}">
        <div
            class="ntp-background-thumbnail ${item.thumbnailClass}">
        </div>
        <div class="option-name">${item.title}</div>
      </button>
    `)}
  </div>

  <div class="button-bar">
    <cr-button id="skipButton" @click="${this.onSkipClicked_}">
      $i18n{skip}
    </cr-button>
    <step-indicator .model="${this.indicatorModel}"></step-indicator>
    <cr-button class="action-button" @click="${this.onNextClicked_}">
      $i18n{next}
      <cr-icon icon="cr:chevron-right"></cr-icon>
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
