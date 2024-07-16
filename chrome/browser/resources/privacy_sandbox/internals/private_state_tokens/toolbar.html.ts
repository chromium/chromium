// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensToolbarElement} from './toolbar.js';

export function getHtml(this: PrivateStateTokensToolbarElement) {
  // clang-format off
  return html`
  <cr-toolbar id="mainToolbar" ?autofocus="${this.autoFocus_}"
      .pageName="${this.pageName}" .searchPrompt="${this.searchPrompt_}"
      .clearLabel="${this.clearLabel_}" .menuLabel="${this.menuLabel_}"
      ?narrow="${this.narrow}" .narrowThreshold="${this.narrowThreshold}"
      ?always-show-logo="${this.alwaysShowLogo_}" ?show-menu="${this.narrow}">
    <cr-icon-button id="helpIcon" iron-icon="cr:help-outline" title="Help">
    </cr-icon-button>
  </cr-toolbar>`;
  // clang-format on
}
