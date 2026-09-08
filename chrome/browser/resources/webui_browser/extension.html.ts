// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionElement} from './extension.js';

export function getHtml(this: ExtensionElement) {
  return html`
    <cr-button type="button"
      @pointerdown="${this.onPointerdown_}"
      @click="${this.onClick}"
      @contextmenu="${this.onContextmenu_}">
      <icon-from-table .iconHandle="${this.iconHandle}"></icon-from-table>
    </cr-button>
  `;
}
