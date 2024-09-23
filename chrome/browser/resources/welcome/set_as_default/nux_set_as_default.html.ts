// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NuxSetAsDefaultElement} from './nux_set_as_default.js';

export function getHtml(this: NuxSetAsDefaultElement) {
  return html`<!--_html_template_start_-->
<div class="container">
  <h1 tabindex="-1">${this.subtitle}</h1>
  <h2>$i18n{setDefaultSubHeader}</h2>
  <div class="illustration slide-in" aria-hidden="true"></div>
  <div class="button-bar">
    <cr-button id="declineButton" @click="${this.onDeclineClick_}">
      $i18n{skip}
    </cr-button>
    <step-indicator .model="${this.indicatorModel}"></step-indicator>
    <cr-button class="action-button" @click="${this.onSetDefaultClick_}">
      $i18n{setDefaultConfirm}
<if expr="is_win">
      <cr-icon icon="cr:open-in-new" slot="suffix-icon"
          ?hidden="${!this.isWin10_}">
      </cr-icon>
</if>
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
