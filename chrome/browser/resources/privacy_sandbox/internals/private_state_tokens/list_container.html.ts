// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensListContainerElement} from './list_container.js';

export function getHtml(this: PrivateStateTokensListContainerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div class="cr-centered-card-container class="flex">
    <h2>$i18n{privateStateTokensHeadingLabel}</h2>
    <div class="flex">
      <p class="inline-text cr-secondary-text">
        $i18n{privateStateTokensDescriptionLabel}
      </p>
      <a href="https://developers.google.com/privacy-sandbox/protections/private-state-tokens"
          target="_blank"
          rel="noopener noreferrer"
          class="inline-text cr-secondary-text">
        $i18n{privateStateTokensExternalLinkLabel}
      </a>
    </div>
    <div class="button-align">
      <cr-button id="expandCollapseButton" @click="${this.onClick_}">
        ${this.expandCollapseButtonText_()}
      </cr-button>
    </div>
    <div class="card" id="private-state-tokens" role="list">
      ${this.data.map((item, index) => html`
          <private-state-tokens-list-item
              .issuerOrigin="${item.issuerOrigin}"
              .numTokens="${item.numTokens}"
              .redemptions="${item.redemptions}"
              @expanded-toggled="${this.onExpandedToggled_}"
              .index="${index}"
              .metadata="${item.metadata}">
          </private-state-tokens-list-item>`)}
    </div>
  </div>
  <!--_html_template_end_-->`;
}