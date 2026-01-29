// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ContextType} from './contextual_tasks.mojom-webui.js';
import type {SourcesMenuElement} from './sources_menu.js';

export function getHtml(this: SourcesMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-action-menu id="menu">
      <div class="header">$i18n{sourcesMenuTitle}</div>
      ${this.contextInfos.map((item, index) => {
        switch (item.type) {
          case ContextType.kTab:
            return html`
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
            `;
          case ContextType.kPdf:
            return html`
              <button class="dropdown-item" @click="${this.onFileClick_}"
                  data-index="${index}">
                <div class="icon-container">
                  <cr-icon icon="thumbnail:pdf" class="file-icon">
                  </cr-icon>
                </div>
                <div class="file-name">${item.title}</div>
              </button>
            `;
          case ContextType.kImage:
            return html`
              <button class="dropdown-item" @click="${this.onImageClick_}"
                  data-index="${index}">
                <div class="image-container">
                  <img is="cr-auto-img" class="image-thumbnail"
                    .autoSrc="${item.url}" aria-label="${item.title}">
                </div>
                <div class="image-title">${item.title}</div>
              </button>
            `;
          default:
            return '';
        }
      })}
    </cr-action-menu>
  <!--_html_template_end_-->`;
  // clang-format on
}
