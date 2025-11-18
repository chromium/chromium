// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopToolbarElement} from './top_toolbar.js';

// clang-format off
export function getHtml(this: TopToolbarElement) {
  return html`<!--_html_template_start_-->
  ${this.title}
  <div id="rightButtonContainer">
    <!-- TODO(crbug.com/454388385): Remove this once the authentication flow
        is implemented. -->
    <button @click="${this.onSigninClick_}">Press for sign in</button>
    <cr-icon-button @click="${this.onNewThreadClick_}" iron-icon="cr:add"
        title="New Thread">
    </cr-icon-button>
    <cr-icon-button @click="${this.onThreadHistoryClick_}"
        iron-icon="cr:history" title="Thread History">
    </cr-icon-button>
    <cr-icon-button @click="${this.onCloseButtonClick_}" iron-icon="cr:close"
    title="Close">
    </cr-icon-button>
    <cr-icon-button id="more" iron-icon="cr:more-vert"
        title="More" @click="${this.onMoreClick_}">
    </cr-icon-button>
  </div>
  <cr-lazy-render-lit id="menu" .template="${() => html`
    <cr-action-menu>
      <button class="dropdown-item" @click="${this.onOpenInNewTabClick_}">
        <cr-icon icon="cr:open-in-new"></cr-icon>
        $i18n{openInNewTab}
      </button>
      <hr>
      <button class="dropdown-item" @click="${this.onOpenChromeSettingsClick_}">
        <cr-icon icon="cr:settings_icon"></cr-icon>
        $i18n{openChromeSettings}
      </button>
      <!-- TODO(crbug.com/459817232): Provide G icon. -->
      <button class="dropdown-item" @click="${this.onMyActivityClick_}">
        <cr-icon icon="cr:history"></cr-icon>
        $i18n{myActivity}
      </button>
      <button class="dropdown-item" @click="${this.onHelpClick_}">
        <cr-icon icon="cr:help-outline"></cr-icon>
        $i18n{help}
      </button>
    </cr-action-menu>`}">
  </cr-lazy-render-lit>
  <!--_html_template_end_-->`;
}
// clang-format on
