// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpComponentsDemoElement} from './sp_components_demo.js';

export function getHtml(this: SpComponentsDemoElement) {
  // clang-format off
  return html`
<h1>Side panel shared components</h1>
<div class="demos">
  Shared UI elements that are only accessible to WebUIs in
  chrome/browser/resources/side_panel.
</div>

<div class="demos">
  <div class="row center">
    <label id="urlCountLabel">Number of url items</label>
    <cr-slider id="urlCountSlider" min="1" max="30" .value="${this.urlCount_}"
        @cr-slider-value-changed="${this.onUrlCountChanged_}"
        aria-labelledby="urlCountLabel">
    </cr-slider>
  </div>
  <cr-checkbox ?checked="${this.hideBackButton_}"
      @checked-changed="${this.onHideBackButtonChanged_}">
    Hide back button in heading
  </cr-checkbox>
  <cr-checkbox ?checked="${this.showBadges_}"
      @checked-changed="${this.onShowBadgesChanged_}">
    Show item badges
  </cr-checkbox>
  <div class="row center">
    <label id="itemSizeLabel">Item size</label>
    <select id="itemSizeSelect" class="md-select"
        aria-labelledby="itemSizeLabel"
        .value="${this.itemSize_}"
        @change="${this.onItemSizeChanged_}">
      ${this.itemSizeOptions_.map(item => html`
        <option .value="${item}">${item}</option>
      `)}
    </select>
  </div>
</div>

<h2>Scrollable container with cards</h2>
<div id="scroller" class="side-panel-demo sp-scroller">
  <div class="sp-card">
    <sp-heading ?hide-back-button="${this.hideBackButton_}"
        back-button-aria-label="$i18n{backButton}">
      <h3 slot="heading">Heading</h3>
    </sp-heading>

    ${this.urls_.map(item => html`
      <cr-url-list-item title="${item.title}"
          description="${item.url}"
          .url="${item.url}"
          .size="${this.itemSize_}">
        ${this.showBadges_ ? html`
          <sp-list-item-badge slot="badges">
            <cr-icon icon="cr:info-outline"></cr-icon>
            <span>2 Notes</span>
          </sp-list-item-badge>
        ` : ''}
      </cr-url-list-item>
    `)}
  </div>
  <hr class="sp-cards-separator">
  <div class="sp-card">
    <sp-heading hide-back-button>
      <h3 slot="heading">Heading</h3>
    </sp-heading>
    <div class="card-content">Some content</div>
  </div>
</div>

<h2>Empty state</h2>
<div id="emptyStateDemo" class="demos">
  <sp-empty-state
      image-path="./demos/side_panel/empty.svg"
      dark-image-path="./demos/side_panel/empty_dark.svg"
      heading="There is no content"
      body="Some more descriptive text explaining how to add content">
  </sp-empty-state>
  <cr-button class="floating-button">
    <cr-icon slot="prefix-icon" icon="cr:add"></cr-icon>
    Add content
  </cr-button>
</div>

<h2>List item badges</h2>
<div class="demos">
  <sp-list-item-badge>
    <cr-icon icon="cr:info-outline"></cr-icon>
    <span>3 Notes</span>
  </sp-list-item-badge>

  <sp-list-item-badge was-updated>
    <cr-icon icon="cr:info-outline"></cr-icon>
    <span>$100</span>
    <span slot="previous-badge">$200</span>
  </sp-list-item-badge>
</div>

<h2>Row of small icon buttons</h2>
<div class="demos">
  <div class="sp-icon-buttons-row">
    <cr-icon-button iron-icon="cr:add"></cr-icon-button>
    <cr-icon-button iron-icon="cr:print"></cr-icon-button>
    <cr-icon-button iron-icon="cr:more-vert"></cr-icon-button>
  </div>
</div>

<h2>Separator</h2>
<div class="demos">
  <hr class="sp-hr">
</div>`;
  // clang-format on
}
