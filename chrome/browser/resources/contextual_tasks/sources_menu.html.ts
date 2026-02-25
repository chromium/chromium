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
      ${this.contextInfos.map((item, index) => html`
        ${item.tab ? html`
          <cr-url-list-item class="dropdown-item" data-index="${index}"
              @click="${this.onTabClick_}"
              .description="${this.getHostname_(item.tab.url)}"
              .url="${item.tab.url}" .title="${item.tab.title}"
              aria-label="${item.tab.title}">
          </cr-url-list-item>
        ` : ''}
        ${item.file && !item.tab ? html`
          <cr-url-list-item class="dropdown-item" data-index="${index}"
              @click="${this.onFileClick_}"
              .url="${item.file.url}" .title="${item.file.title}">
            <cr-icon slot="customIcon" icon="contextual_tasks:pdf"
                class="file-icon">
            </cr-icon>
          </cr-url-list-item>
        ` : ''}
        ${item.image && !item.tab && !item.file ? html`
          <cr-url-list-item class="dropdown-item" data-index="${index}"
              @click="${this.onImageClick_}"
              .title="${item.image.title}">
            <cr-icon slot="customIcon" icon="contextual_tasks:img_icon">
            </cr-icon>
          </cr-url-list-item>
        ` : ''}
      `)}
    </cr-action-menu>
  <!--_html_template_end_-->`;
  // clang-format on
}
