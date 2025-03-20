// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsSectionElement} from './extensions_section.js';

export function getHtml(this: ExtensionsSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="extensionsSection">
  <div id="header-row">
    <cr-checkbox id="checkbox" aria-label-override="${this.title_}">
    </cr-checkbox>
    <div id="title-container">
      <span id="title">${this.title_}</span>
      <!-- Make the icon focusable since it has a tooltip. cr-icon is used here
      to support custom tooltip widths. -->
      <cr-icon id="tooltip-icon"
          icon="cr:info-outline"
          tabindex="0"
          role="img"
          aria-label="${this.tooltip_}">
      </cr-icon>
    </div>
    <cr-expand-button id="expandButton" no-hover
        ?expanded="${this.expanded_}"
        @expanded-changed="${this.onExpandChanged_}"
        role="button"
        aria-label="${this.title_}">
    </cr-expand-button>
  </div>
  <cr-tooltip id="tooltip" for="tooltip-icon" position="right" offset="0"
      aria-hidden="true" animation-delay="0" fit-to-visible-bounds>
    ${this.tooltip_}
  </cr-tooltip>
  <cr-collapse id="collapse" ?opened="${this.expanded_}">
    <div id="account-extensions-list" class="custom-scrollbar">
      ${this.accountExtensions.map((extension) => html`
        <div class="account-extension">
          <div class="item-icon-container">
            <img class="item-icon" alt="" src="${extension.iconUrl}">
          </div>
          <span class="name">${extension.name}</span>
        </div>
      `)}
    </div>
  </cr-collapse>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
