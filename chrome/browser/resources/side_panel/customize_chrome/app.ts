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

import {HelpBubbleMixin, HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {AppearanceElement} from './appearance.js';
import {CategoriesElement} from './categories.js';
import {ChromeColorsElement} from './chrome_colors.js';
import {BackgroundCollection, CustomizeChromeSection} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {ThemesElement} from './themes.js';

const SECTION_TO_SELECTOR = {
  [CustomizeChromeSection.kAppearance]: '#appearance',
  [CustomizeChromeSection.kShortcuts]: '#shortcuts',
  [CustomizeChromeSection.kModules]: '#modules',
};

const CHANGE_CHROME_THEME_BUTTON_ELEMENT_ID =
    'CustomizeChromeUI::kChangeChromeThemeButtonElementId';

export enum CustomizeChromePage {
  OVERVIEW = 'overview',
  CATEGORIES = 'categories',
  THEMES = 'themes',
  CHROME_COLORS = 'chrome-colors',
}

const AppElementBase = HelpBubbleMixin(PolymerElement) as
    {new (): PolymerElement & HelpBubbleMixinInterface};

export interface AppElement {
  $: {
    overviewPage: HTMLDivElement,
    categoriesPage: CategoriesElement,
    themesPage: ThemesElement,
    appearanceElement: AppearanceElement,
    chromeColorsPage: ChromeColorsElement,
  };
}

export class AppElement extends AppElementBase {
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

  override ready() {
    super.ready();
    this.registerHelpBubble(
        CHANGE_CHROME_THEME_BUTTON_ELEMENT_ID,
        ['#appearanceElement', '#editThemeButton']);
  }

  private page_: CustomizeChromePage;
  private selectedCollection_: BackgroundCollection|null;
  private scrollToSectionListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.scrollToSectionListenerId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.scrollToSection.addListener(
                (section: CustomizeChromeSection) => {
                  const selector = SECTION_TO_SELECTOR[section];
                  const element = this.shadowRoot!.querySelector(selector);
                  if (!element) {
                    return;
                  }
                  this.page_ = CustomizeChromePage.OVERVIEW;
                  element.scrollIntoView({behavior: 'auto'});
                });
    // We wait for load because `scrollIntoView` above requires the page to be
    // laid out.
    window.addEventListener('load', () => {
      CustomizeChromeApiProxy.getInstance().handler.updateScrollToSection();
    }, {once: true});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.scrollToSectionListenerId_);
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.scrollToSectionListenerId_);
  }

  private onBackClick_() {
    switch (this.page_) {
      case CustomizeChromePage.CATEGORIES:
        this.page_ = CustomizeChromePage.OVERVIEW;
        this.$.appearanceElement.focusOnThemeButton();
        break;
      case CustomizeChromePage.THEMES:
      case CustomizeChromePage.CHROME_COLORS:
        this.page_ = CustomizeChromePage.CATEGORIES;
        this.$.categoriesPage.focusOnBackButton();
        break;
    }
  }

  private onEditThemeClick_() {
    this.page_ = CustomizeChromePage.CATEGORIES;
    this.$.categoriesPage.focusOnBackButton();
  }

  private onCollectionSelect_(event: CustomEvent<BackgroundCollection>) {
    this.selectedCollection_ = event.detail;
    this.page_ = CustomizeChromePage.THEMES;
    this.$.themesPage.focusOnBackButton();
  }

  private onLocalImageUpload_() {
    this.page_ = CustomizeChromePage.OVERVIEW;
    this.$.appearanceElement.focusOnThemeButton();
  }

  private onChromeColorsSelect_() {
    this.page_ = CustomizeChromePage.CHROME_COLORS;
    this.$.chromeColorsPage.focusOnBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
