// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-page-selector id="container"
    class="sp-scroller sp-scroller-top-of-page sp-scroller-bottom-of-page"
    selected="${this.page_}" attr-for-selected="page-name">
  <div page-name="overview" id="overviewPage">
    <div id="appearance" class="section sp-card">
      <sp-heading hide-back-button>
        <h2 slot="heading">$i18n{appearanceHeader}</h2>
      </sp-heading>
      <customize-chrome-appearance @edit-theme-click="${this.onEditThemeClick_}"
          @wallpaper-search-click="${this.onWallpaperSearchSelect_}"
          id="appearanceElement">
      </customize-chrome-appearance>
    </div>
    ${this.toolbarCustomizationEnabled_ ? html`
      <hr class="sp-cards-separator">
      <cr-button id="toolbarButton" class="section sp-card"
          @click="${this.onToolbarCustomizationButtonClick_}">
        <sp-heading hide-back-button id="toolbar-customization-heading">
          <h2 slot="heading" id="toolbar-customization-inner-heading"
              aria-label="$i18n{toolbarButtonA11yLabel}">
            $i18n{toolbarHeader}
            <new-badge></new-badge>
          </h2>
        </sp-heading>
        <cr-icon icon="cr:chevron-right" slot="suffix-icon"></cr-icon>
      </cr-button>
    ` : ''}
     ${this.isSourceTabFirstPartyNtp_ ? html`<hr class="sp-cards-separator">
    <div id="shortcuts" class="section sp-card">
      <sp-heading hide-back-button>
        <h2 slot="heading">$i18n{shortcutsHeader}</h2>
      </sp-heading>
      <customize-chrome-shortcuts></customize-chrome-shortcuts>
    </div>`: ''}
    ${(this.modulesEnabled_ && this.isSourceTabFirstPartyNtp_) ? html`
      <hr class="sp-cards-separator">
      <div id="modules" class="section sp-card">
        <sp-heading hide-back-button>
          <h2 slot="heading">$i18n{cardsHeader}</h2>
        </sp-heading>
        <customize-chrome-cards></customize-chrome-cards>
      </div>
    ` : ''}
    ${this.extensionsCardEnabled_ ? html`
      <hr class="sp-cards-separator">
      <div id="extensions" class="section sp-card">
        <sp-heading hide-back-button>
          <h2 slot="heading">Extensions</h2>
        </sp-heading>
        <div class="description" @click="${this.onChromeWebStoreLinkClick_}">
          $i18nRaw{customizeWithChromeWebstoreLabel}
        </div>
        <div id="buttonContainer">
          <cr-chip id="couponsButton" chip-role="link"
            @click="${this.onCouponsButtonClick_}">
            <div class="cr-icon"></div>
            $i18n{webstoreShoppingCategoryLabel}
          </cr-chip>
          <cr-chip id="writingButton" chip-role="link"
            @click="${this.onWritingButtonClick_}">
            <div class="cr-icon"></div>
            $i18n{webstoreWritingHelpCollectionLabel}
          </cr-chip>
          <cr-chip id="productivityButton" chip-role="link"
            @click="${this.onProductivityButtonClick_}">
            <div class="cr-icon"></div>
            $i18n{webstoreProductivityCategoryLabel}
          </cr-chip>
        </div>
      </div>
    ` : ''}
  </div>
  ${(this.isSourceTabFirstPartyNtp_) ? html`
  <customize-chrome-categories @back-click="${this.onBackClick_}"
      @collection-select="${this.onCollectionSelect_}" page-name="categories"
      id="categoriesPage" @local-image-upload="${this.onLocalImageUpload_}"
      @wallpaper-search-select="${this.onWallpaperSearchSelect_}">
  </customize-chrome-categories>`: ''}
  ${(this.isSourceTabFirstPartyNtp_) ? html`
  <customize-chrome-themes @back-click="${this.onBackClick_}"
      page-name="themes" id="themesPage"
      .selectedCollection="${this.selectedCollection_}">
  </customize-chrome-themes>`: ''}
  ${(this.wallpaperSearchEnabled_ && this.isSourceTabFirstPartyNtp_) ? html`
    <customize-chrome-wallpaper-search @back-click="${this.onBackClick_}"
        page-name="wallpaper-search" id="wallpaperSearchPage">
    </customize-chrome-wallpaper-search>
  ` : ''}
  ${this.toolbarCustomizationEnabled_ ? html`
    <customize-chrome-toolbar @back-click="${this.onBackClick_}"
      page-name="toolbar" id="toolbarPage"></customize-chrome-toolbar>
  ` : ''}
</cr-page-selector>
<!--_html_template_end_-->`;
  // clang-format on
}
