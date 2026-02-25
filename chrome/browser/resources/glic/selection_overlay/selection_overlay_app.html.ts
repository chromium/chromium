// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SelectionOverlayAppElement} from './selection_overlay_app.js';

export function getHtml(this: SelectionOverlayAppElement) {
  return html`
    <cr-button id="closeButton" @click="${this.onCloseClick_}">
      Close
    </cr-button>
  `;
}
