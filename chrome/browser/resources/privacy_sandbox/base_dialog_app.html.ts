// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BaseDialogApp} from './base_dialog_app.js';

export function getHtml(this: BaseDialogApp) {
  return html`
    <div>Base Dialog App Placeholder</div>
    <cr-button id="closeButton" @click="${this.onCloseButton_}">
      Close
    </cr-button>
  `;
}
