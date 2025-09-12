// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionElement} from './extension_element.js';

export function getHtml(this: ExtensionElement) {
  return html`
    <cr-button type="button"
      @click="${this.onClick}"
      @contextmenu="${this.onContextMenu}">
      <div id="icon"></div>
    </cr-button>
  `;
}
