// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ThemeSnapshotElement} from './theme_snapshot.js';
import {CustomizeThemeType} from './theme_snapshot.js';

export function getHtml(this: ThemeSnapshotElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.themeType_ === CustomizeThemeType.CUSTOM_THEME ? html`
  <div class="snapshot-container"
      theme-type="${CustomizeThemeType.CUSTOM_THEME}">
    <div class="image-background" id="customThemeImageBackground"
        @click="${this.onThemeSnapshotClick_}">
      <img class="image" id="customThemeImage" is="cr-auto-img"
          .autoSrc="${this.theme_!.backgroundImage!.snapshotUrl.url}"
          draggable="false"
          aria-labelledby="customThemeTitle">
      <div class="overlay"></div>
      <cr-ripple></cr-ripple>
    </div>
    <div id="customThemeTitle" class="theme-title">
     ${this.theme_!.backgroundImage!.title}
    </div>
  </div>
` : ''}
${this.themeType_ === CustomizeThemeType.CLASSIC_CHROME ? html`
  <div class="snapshot-container"
      theme-type="${CustomizeThemeType.CLASSIC_CHROME}">
    <div class="image-background image" id="classicChromeBackground"
        @click="${this.onThemeSnapshotClick_}">
      <svg id="miniNewTabPage" aria-labelledby="classicChromeThemeTitle"
          viewBox="0 0 240 126" preserveAspectRatio="xMidYMid meet">
        <use href="icons/mini_new_tab_page.svg#miniNewTabPage"></use>
      </svg>
      <div class="overlay"></div>
      <cr-ripple></cr-ripple>
    </div>
    <div id="classicChromeThemeTitle" class="theme-title">
      $i18n{classicChrome}
    </div>
  </div>
` : ''}
${this.themeType_ === CustomizeThemeType.UPLOADED_IMAGE ? html`
  <div class="snapshot-container"
      theme-type="${CustomizeThemeType.UPLOADED_IMAGE}">
    <div class="image-background" id="uploadedThemeImageBackground"
        @click="${this.onThemeSnapshotClick_}">
      <img class="image" id="uploadedThemeImage" src="icons/uploaded_image.svg"
          aria-labelledby="uploadedThemeTitle">
      <div class="overlay"></div>
      <cr-ripple></cr-ripple>
    </div>
    <div id="uploadedThemeTitle" class="theme-title">$i18n{uploadedImage}</div>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
