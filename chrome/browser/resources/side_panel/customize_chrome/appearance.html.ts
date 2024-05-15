// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppearanceElement} from './appearance.js';

export function getHtml(this: AppearanceElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<customize-chrome-theme-snapshot id="themeSnapshot"
    @edit-theme-click="${this.onEditThemeClicked_}"
    ?hidden="${!this.showThemeSnapshot_}">
</customize-chrome-theme-snapshot>
<customize-chrome-hover-button id="thirdPartyLinkButton" class="theme-button"
    ?hidden="${!this.thirdPartyThemeId_}"
    @click="${this.onThirdPartyLinkButtonClick_}"
    label="${this.thirdPartyThemeName_}"
    label-description="$i18n{currentTheme}">
</customize-chrome-hover-button>
<customize-chrome-hover-button id="uploadedImageButton" class="theme-button"
    ?hidden="${!this.showUploadedImageButton_}"
    @click="${this.onUploadedImageButtonClick_}"
    label="$i18n{yourUploadedImage}"
    label-description="$i18n{currentTheme}">
</customize-chrome-hover-button>
<customize-chrome-hover-button id="searchedImageButton" class="theme-button"
    ?hidden="${!this.showSearchedImageButton_}"
    @click="${this.onSearchedImageButtonClick_}"
    label="$i18n{yourSearchedImage}"
    label-description="$i18n{currentTheme}">
</customize-chrome-hover-button>
<div id="editButtonsContainer">
  <cr-button id="editThemeButton" @click="${this.onEditThemeClicked_}"
      class="floating-button">
    <div id="editThemeIcon" class="cr-icon edit-theme-icon" slot="prefix-icon"
        ?hidden="${this.wallpaperSearchButtonEnabled_}"></div>
    ${this.editThemeButtonText_}
  </cr-button>
  ${this.wallpaperSearchButtonEnabled_ ? html`
    <cr-button id="wallpaperSearchButton"
        @click="${this.onWallpaperSearchClicked_}" class="floating-button">
      <div id="wallpaperSearchIcon" class="cr-icon edit-theme-icon"
          slot="prefix-icon"></div>
      $i18n{wallpaperSearchTileLabel}
    </cr-button>
  ` : ''}
</div>
<hr class="sp-hr">
<customize-color-scheme-mode></customize-color-scheme-mode>
<cr-theme-color-picker id="chromeColors" ?hidden="${!this.showColorPicker_}">
</cr-theme-color-picker>
<hr class="sp-hr" ?hidden="${!this.showBottomDivider_}">
<div id="followThemeToggle" class="sp-card-content"
    ?hidden="${!this.showDeviceThemeToggle_}">
  <div id="followThemeToggleTitle">$i18n{followThemeToggle}</div>
  <cr-toggle id="followThemeToggleControl" title="$i18n{followThemeToggle}"
      ?checked="${!!this.theme_ && this.theme_.followDeviceTheme}"
      @change="${this.onFollowThemeToggleChange_}">
  </cr-toggle>
</div>
<customize-chrome-hover-button id="setClassicChromeButton"
    ?hidden="${!this.showClassicChromeButton_}"
    label="$i18n{resetToClassicChrome}"
    @click="${this.onSetClassicChromeClicked_}">
</customize-chrome-hover-button>
${this.showManagedDialog_ ? html`
  <managed-dialog @close="${this.onManagedDialogClosed_}"
      title="$i18n{managedColorsTitle}"
      body="$i18n{managedColorsBody}">
  </managed-dialog>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
