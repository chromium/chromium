// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensAppElement} from './app.js';

export function getHtml(this: PrivateStateTokensAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <private-state-tokens-toolbar id="toolbar" .pageName="${this.pageTitle_}"
      ?narrow="${this.narrow_}"
      .narrowThreshold="${this.narrowThreshold_}">
  </private-state-tokens-toolbar>
  <div id="container" role="group">
    <private-state-tokens-sidebar id="sidebar" ?hidden="${this.narrow_}">
    </private-state-tokens-sidebar>
    <div id="content">
      <private-state-tokens-navigation .metadata_=${this.metadata_}
          .itemToRender=${this.itemToRender} .data=${this.data}>
      </private-state-tokens-navigation>
    </div>
    <div id="space-holder" ?hidden="${this.narrow_}"></div>
    <cr-drawer
        id="drawer"
        heading="Private State Tokens"
        @close="${this.onDrawerClose_}">
      <div slot="body">
        <private-state-tokens-sidebar></private-state-tokens-sidebar>
      </div>
    </cr-drawer>
  </div>
  <!--_html_template_end_-->`;
  //clang-format on
}
