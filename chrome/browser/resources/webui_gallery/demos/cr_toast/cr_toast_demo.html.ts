// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrToastDemoElement} from './cr_toast_demo.js';

export function getHtml(this: CrToastDemoElement) {
  return html`
<h1>cr-toast</h1>
<div class="demos">
  <cr-input type="text" label="Toast message" .value="${this.message_}"
      @value-changed="${this.onMessageChanged_}"></cr-input>
  <cr-checkbox ?checked="${this.showDismissButton_}"
      @checked-changed="${this.onShowDismissButtonChanged_}">
    Show dismiss button
  </cr-checkbox>
  <cr-input type="number" label="Duration (ms)" .value="${this.duration_}"
      @value-changed="${this.onDurationChanged_}">
  </cr-input>

  <cr-button @click="${this.onShowToastClick_}">Show toast</cr-button>
</div>

<h1>cr-toast-manager</h1>
<div class="demos">
  <div>
    One single toast manager per top-level app that allows for more dynamic
    toast messages that can be truncated if strings are too long.
  </div>

  <cr-button @click="${this.onShowToastManagerClick_}">
    Show toast from toast manager
  </cr-button>
</div>

<cr-toast id="toast" duration="${this.duration_}">
  <div id="toastContent">${this.message_}</div>
  <cr-button ?hidden="${!this.showDismissButton_}"
      @click="${this.onHideToastClick_}">
    Dismiss
  </cr-button>
</cr-toast>

<cr-toast-manager .duration="${this.duration_}"></cr-toast-manager>`;
}
