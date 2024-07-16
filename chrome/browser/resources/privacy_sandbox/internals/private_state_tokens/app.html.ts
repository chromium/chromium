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
      <div class="cr-centered-card-container class="flex">
          <h2>$i18n{privateStateTokensHeadingLabel}</h2>
          <div class="flex">
            <p class="inline-text">
              $i18n{privateStateTokensDescriptionLabel}
            </p>
            <a
                href="https://developers.google.com/privacy-sandbox/protections/private-state-tokens"
                target="_blank"
                rel="noopener noreferrer"
                class="inline-text">
              $i18n{privateStateTokensExternalLinkLabel}
            </a>
          </div>
          <div class="button-align">
            <cr-button>Outline button</cr-button>
          </div>
          <div class="card">
            ${this.data_.map(item => html`
                <private-state-tokens-list-item
                    .issuerOrigin="${item.issuerOrigin}"
                    .numTokens="${item.numTokens}"
                    .redemptions="${item.redemptions}">
                </private-state-tokens-list-item>`)}
          </div>
      </div>
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