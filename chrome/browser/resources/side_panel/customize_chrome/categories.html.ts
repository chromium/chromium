// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CategoriesElement} from './categories.js';

export function getHtml(this: CategoriesElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="sp-card">
  <sp-heading id="heading" @back-button-click="${this.onBackClick_}"
      back-button-aria-label="$i18n{backButton}"
      back-button-title="$i18n{backButton}">
    <h2 slot="heading">$i18n{categoriesHeader}</h2>
  </sp-heading>
  <cr-grid columns="6" disable-arrow-navigation>
    <div class="tile" tabindex="0" id="classicChromeTile"
        role="button" @click="${this.onClassicChromeClick_}"
        aria-current="${this.isClassicChromeSelected_}">
      <customize-chrome-check-mark-wrapper
          ?checked="${this.isClassicChromeSelected_}">
        <div id="cornerNewTabPageTile" class="image-container">
          <img id="cornerNewTabPage" src="icons/corner_new_tab_page.svg">
        </div>
      </customize-chrome-check-mark-wrapper>
      <div class="label">$i18n{classicChrome}</div>
    </div>
    ${this.wallpaperSearchEnabled_ ? html`
      <div class="tile" tabindex="0" id="wallpaperSearchTile"
          role="button" @click="${this.onWallpaperSearchClick_}"
          aria-current="${this.isWallpaperSearchSelected_}">
        <customize-chrome-check-mark-wrapper
            ?checked="${this.isWallpaperSearchSelected_}">
          <div class="image-container">
            <customize-chrome-wallpaper-search-tile>
            </customize-chrome-wallpaper-search-tile>
            <div id="wallpaperSearchIcon" class="cr-icon"></div>
          </div>
        </customize-chrome-check-mark-wrapper>
        <div class="label">$i18n{wallpaperSearchTileLabel}</div>
      </div>
    ` : ''}
    <div class="tile" tabindex="0" id="uploadImageTile"
        role="button" @click="${this.onUploadImageClick_}"
        aria-current="${this.isLocalImageSelected_}">
      <customize-chrome-check-mark-wrapper
          ?checked="${this.isLocalImageSelected_}">
        <div class="image-container">
          <div id="uploadIcon" class="cr-icon"></div>
        </div>
      </customize-chrome-check-mark-wrapper>
      <div class="label">$i18n{uploadImage}</div>
    </div>
    ${this.collections_.map((item, index) => html`
      <div class="tile collection" tabindex="0" role="button"
          data-index="${index}" @click="${this.onCollectionClick_}"
          aria-current="${this.isCollectionSelected_(item.id)}"
          ?hidden="${!this.shouldShowCollection_(item.imageVerified)}">
        <customize-chrome-check-mark-wrapper
            ?checked="${this.isCollectionSelected_(item.id)}">
          <div class="image-container">
            <img is="cr-auto-img" data-index="${index}"
                auto-src="${item.previewImageUrl.url}"
                draggable="false"
                @load="${this.onPreviewImageLoad_}"
                @error="${this.onPreviewImageError_}">
            </img>
          </div>
        </customize-chrome-check-mark-wrapper>
        <div class="label">${item.label}</div>
      </div>
    `)}
    <div class="tile" tabindex="0" role="button"
        @click="${this.onChromeWebStoreClick_}" id="chromeWebStoreTile">
      <div class="image-container">
        <img id="chromeWebStore" src="icons/chrome_web_store.svg"></img>
      </div>
      <div class="label">
        <div class="cr-icon icon-external"></div>
        $i18n{chromeWebStore}
      </div>
    </div>
  </cr-grid>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
