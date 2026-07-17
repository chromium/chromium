// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BannerPromoElement} from './banner_promo.js';

export function getHtml(this: BannerPromoElement) {
  return html`
    <div id="container">
      <div class="content-wrapper">
        <div class="header">
          <cr-icon
              icon="${this.webuiRoundedIconsEnabled_
                  ? 'contextual_tasks:screensaver-auto'
                  : 'contextual_tasks:screensaver-auto-old'}"></cr-icon>
          <slot name="header"></slot>
        </div>
        <div class="body"><slot name="body"></slot></div>
        <div class="buttons">
          <cr-button class="tonal-button" @click="${this.onNotNowClick_}">
            ${this.dismissButtonText}
          </cr-button>
          <cr-button class="action-button" @click="${this.onTurnOnClick_}">
            ${this.acceptButtonText}
          </cr-button>
        </div>
      </div>
    </div>
  `;
}
