// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HeaderElement} from './header.js';

// clang-format off
export function getHtml(this: HeaderElement) {
  return html`<!--_html_template_start_-->
  <img id="logo" srcset="chrome://theme/current-channel-logo@4x 4x"
      role="presentation">
  <div class="title-text" id="title"
      @click="${this.onPageTitleClick_}">
    $i18n{pageTitle}
  </div>
  <div id="divider" ?hidden="${!this.subtitle}"></div>
  ${this.showingInput_ ? html`
    <cr-input id="input" class="stroked" value="${this.subtitle}"
        @keydown="${this.onInputKeyDown_}"
        @blur="${this.onInputBlur_}"
        maxlength="${this.maxNameLength_}"
        aria-label="$i18n{tableNameInputA11yLabel}">
    </cr-input>
    <div class="spacer"></div>
    ` : html`
    <div class="title-text" id="subtitle"
        ?hidden="${!this.subtitle}"
        @click="${this.onRenaming_}"
        @keydown="${this.onSubtitleKeyDown_}"
        role="textbox"
        tabindex="0">
      ${this.subtitle}
    </div>
    `}

  <cr-icon-button id="menuButton" class="icon-more-vert"
      @click="${this.showMenu_}"
      ?disabled="${this.disabled}"
      ?hidden="${!this.subtitle}"
      aria-label="$i18n{tableMenuA11yLabel}">
  </cr-icon-button>

  <header-menu id="menu" @close="${this.onCloseMenu_}"
      @rename-click="${this.onRenaming_}">
  </header-menu>
  <!--_html_template_end_-->`;
}
// clang-format on
