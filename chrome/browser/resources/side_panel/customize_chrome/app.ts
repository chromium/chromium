// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './appearance.js';
import './cards.js';
import './categories.js';
import './chrome_colors.js';
import './shortcuts.js';
import './themes.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {AppearanceElement} from './appearance.js';
import {CategoriesElement} from './categories.js';
import {ChromeColorsElement} from './chrome_colors.js';
import {BackgroundCollection} from './customize_chrome.mojom-webui.js';
import {ThemesElement} from './themes.js';

export enum CustomizeChromePage {
  OVERVIEW = 'overview',
  CATEGORIES = 'categories',
  THEMES = 'themes',
  CHROME_COLORS = 'chrome-colors',
}

export interface AppElement {
  $: {
    overviewPage: HTMLDivElement,
    categoriesPage: CategoriesElement,
    themesPage: ThemesElement,
    appearanceElement: AppearanceElement,
    chromeColorsPage: ChromeColorsElement,
  };
}

export class AppElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      page_: {
        type: String,
        value: CustomizeChromePage.OVERVIEW,
      },
      modulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesEnabled'),
      },
      selectedCollection_: {
        type: Object,
        value: null,
      },
    };
  }

  private page_: CustomizeChromePage;
  private selectedCollection_: BackgroundCollection|null;

  private onBackClick_() {
    switch (this.page_) {
      case CustomizeChromePage.CATEGORIES:
        this.page_ = CustomizeChromePage.OVERVIEW;
        break;
      case CustomizeChromePage.THEMES:
        this.page_ = CustomizeChromePage.CATEGORIES;
        break;
      case CustomizeChromePage.CHROME_COLORS:
        this.page_ = CustomizeChromePage.CATEGORIES;
        break;
    }
  }

  private onEditThemeClick_() {
    this.page_ = CustomizeChromePage.CATEGORIES;
  }

  private onCollectionSelect_(event: CustomEvent<BackgroundCollection>) {
    this.selectedCollection_ = event.detail;
    this.page_ = CustomizeChromePage.THEMES;
  }

  private onLocalImageUpload_() {
    this.page_ = CustomizeChromePage.OVERVIEW;
  }

  private onChromeColorsSelect_() {
    this.page_ = CustomizeChromePage.CHROME_COLORS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
