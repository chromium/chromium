// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SourcesMenuElement} from './sources_menu.js';

export function getHtml(this: SourcesMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-action-menu id="menu">
      <div class="header">$i18n{sourcesMenuTitle}</div>
      ${this.attachedTabs.map((item, index) => html`
        <button class="dropdown-item" @click="${this.onTabClick_}"
            data-index="${index}">
          <div class="icon-container">
            <div class="tab-favicon"
              style="background-image:${this.faviconUrl_(item)}">
            </div>
          </div>
          <div class="tab-info">
            <div class="tab-title">${item.title}</div>
            <div class="tab-url">${this.getHostname_(item.url)}</div>
          </div>
        </button>
      `)}
      ${this.attachedImages.map((item, index) => html`
        <button class="dropdown-item" @click="${this.onImageClick_}"
            data-index="${index}">
          <div class="image-container">
            <img is="cr-auto-img" class="image-thumbnail"
              .autoSrc="${item.url}" aria-label="${item.title}">
          </div>
          <div class="image-title">${item.title}</div>
        </button>
      `)}
      ${this.attachedFiles.map((item, index) => html`
        <button class="dropdown-item" @click="${this.onFileClick_}"
            data-index="${index}">
          <div class="icon-container">
            <cr-icon icon="thumbnail:pdf" class="file-icon">
            </cr-icon>
          </div>
          <div class="file-name">${item.name}</div>
        </button>
      `)}
    </cr-action-menu>
  <!--_html_template_end_-->`;
  // clang-format on
}
