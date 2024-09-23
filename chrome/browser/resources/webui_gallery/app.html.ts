// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebuiGalleryAppElement} from './app.js';

export function getHtml(this: WebuiGalleryAppElement) {
  return html`
<div id="sidebar">
  <h1>WebUI Gallery</h1>
  <cr-menu-selector id="selector" selectable="a" selected-attribute="selected"
      @iron-select="${this.onMenuItemSelect_}">
    ${this.demos_.map(demo => html`
        <a role="menuitem" href="${demo.path}"
            class="cr-nav-menu-item" @click="${this.onMenuItemClick_}">
        ${demo.name}
        </a>`)}
  </cr-menu-selector>

  <div class="cr-row">
    <span id="toggleDescription">Follow color pipeline</span>
    <cr-toggle aria-labeledby="toggleDescription"
        @change="${this.onFollowColorPipelineChange_}">
    </cr-toggle>
  </div>
</div>

<div id="main"></div>`;
}
