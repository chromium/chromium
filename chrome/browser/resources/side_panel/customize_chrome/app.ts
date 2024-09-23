// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_components/help_bubble/new_badge.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './appearance.js';
import './cards.js';
import './categories.js';
import './customize_toolbar/toolbar.js';
import './shortcuts.js';
import './themes.js';
import './wallpaper_search/wallpaper_search.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AppearanceElement} from './appearance.js';
import type {CategoriesElement} from './categories.js';
import {CustomizeChromeImpression, recordCustomizeChromeImpression} from './common.js';
import type {BackgroundCollection, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {ChromeWebStoreCategory, ChromeWebStoreCollection, CustomizeChromeSection} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import type {ThemesElement} from './themes.js';

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
  TOOLBAR = 'toolbar',
  WALLPAPER_SEARCH = 'wallpaper-search',
}

const AppElementBase = HelpBubbleMixinLit(CrLitElement);

export interface AppElement {
  $: {
    overviewPage: HTMLDivElement,
    categoriesPage: CategoriesElement,
    themesPage: ThemesElement,
    appearanceElement: AppearanceElement,
  };
}

export class AppElement extends AppElementBase {
  static get is() {
    return 'customize-chrome-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      page_: {type: String},
      modulesEnabled_: {type: Boolean},
      selectedCollection_: {type: Object},
      extensionsCardEnabled_: {type: Boolean},
      wallpaperSearchEnabled_: {type: Boolean},
      toolbarCustomizationEnabled_: {type: Boolean},
      isSourceTabFirstPartyNtp_: {type: Boolean},
    };
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
    this.registerHelpBubble(
        CHANGE_CHROME_THEME_BUTTON_ELEMENT_ID,
        ['#appearanceElement', '#editThemeButton']);
  }

  protected page_: CustomizeChromePage = CustomizeChromePage.OVERVIEW;
  protected modulesEnabled_: boolean =
      loadTimeData.getBoolean('modulesEnabled');
  protected selectedCollection_: BackgroundCollection|null = null;
  protected extensionsCardEnabled_: boolean =
      loadTimeData.getBoolean('extensionsCardEnabled');
  protected wallpaperSearchEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchEnabled');
  protected toolbarCustomizationEnabled_: boolean =
      loadTimeData.getBoolean('toolbarCustomizationEnabled');
  protected isSourceTabFirstPartyNtp_: boolean = true;
  private scrollToSectionListenerId_: number|null = null;
  private attachedTabStateUpdatedId_: number|null = null;
  private pageHandler_: CustomizeChromePageHandlerInterface =
      CustomizeChromeApiProxy.getInstance().handler;

  override connectedCallback() {
    super.connectedCallback();
    this.scrollToSectionListenerId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.scrollToSection.addListener(
                (section: CustomizeChromeSection) => {
                  if (section === CustomizeChromeSection.kWallpaperSearch) {
                    this.onWallpaperSearchSelect_();
                    return;
                  } else if (section === CustomizeChromeSection.kToolbar) {
                    this.openToolbarCustomizationPage();
                    chrome.metricsPrivate.recordUserAction(
                        'Actions.CustomizeToolbarSidePanel' +
                        '.OpenedFromOutsideCustomizeChrome');
                    return;
                  }
                  const selector = SECTION_TO_SELECTOR[section];
                  const element = this.shadowRoot!.querySelector(selector);
                  if (!element) {
                    return;
                  }
                  this.page_ = CustomizeChromePage.OVERVIEW;
                  element.scrollIntoView({behavior: 'auto'});
                });

    this.attachedTabStateUpdatedId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.attachedTabStateUpdated.addListener(
                (isSourceTabFirstPartyNtp: boolean) => {
                  if (this.isSourceTabFirstPartyNtp_ ===
                      isSourceTabFirstPartyNtp) {
                    return;
                  }

                  this.isSourceTabFirstPartyNtp_ = isSourceTabFirstPartyNtp;

                  // Since some pages aren't supported in non first party mode,
                  // change the section back to the overview.
                  if (!this.isSourceTabFirstPartyNtp_ &&
                      !this.pageSupportedOnNonFirstPartyNtps()) {
                    this.page_ = CustomizeChromePage.OVERVIEW;
                  }
                });
    this.pageHandler_.updateAttachedTabState();

    // We wait for load because `scrollIntoView` above requires the page to be
    // laid out.
    window.addEventListener('load', () => {
      CustomizeChromeApiProxy.getInstance().handler.updateScrollToSection();
      // Install observer to log extension cards impression.
      const extensionsCardSectionObserver =
          new IntersectionObserver(entries => {
            assert(entries.length >= 1);
            if (entries[0]!.intersectionRatio >= 0.8) {
              extensionsCardSectionObserver.disconnect();
              this.dispatchEvent(
                  new Event('detect-extensions-card-section-impression'));
              recordCustomizeChromeImpression(
                  CustomizeChromeImpression.EXTENSIONS_CARD_SECTION_DISPLAYED);
            }
          }, {
            threshold: 1.0,
          });
      // Start observing if extension cards are scroll into view.
      if (this.shadowRoot && this.shadowRoot.querySelector('#extensions')) {
        extensionsCardSectionObserver.observe(
            this.shadowRoot!.querySelector('#extensions')!);
      }
    }, {once: true});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.scrollToSectionListenerId_);
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.scrollToSectionListenerId_);

    assert(this.attachedTabStateUpdatedId_);
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.attachedTabStateUpdatedId_);
  }

  protected async onBackClick_() {
    switch (this.page_) {
      case CustomizeChromePage.CATEGORIES:
      case CustomizeChromePage.TOOLBAR:
        this.page_ = CustomizeChromePage.OVERVIEW;
        await this.updateComplete;
        this.$.appearanceElement.focusOnThemeButton();
        break;
      case CustomizeChromePage.THEMES:
      case CustomizeChromePage.WALLPAPER_SEARCH:
        this.page_ = CustomizeChromePage.CATEGORIES;
        await this.updateComplete;
        this.$.categoriesPage.focusOnBackButton();
        break;
    }
  }

  protected async onEditThemeClick_() {
    this.page_ = CustomizeChromePage.CATEGORIES;
    await this.updateComplete;
    this.$.categoriesPage.focusOnBackButton();
  }

  protected async onCollectionSelect_(event:
                                          CustomEvent<BackgroundCollection>) {
    this.selectedCollection_ = event.detail;
    this.page_ = CustomizeChromePage.THEMES;
    await this.updateComplete;
    this.$.themesPage.focusOnBackButton();
  }

  protected async onLocalImageUpload_() {
    this.page_ = CustomizeChromePage.OVERVIEW;
    await this.updateComplete;
    this.$.appearanceElement.focusOnThemeButton();
  }

  protected onWallpaperSearchSelect_() {
    this.page_ = CustomizeChromePage.WALLPAPER_SEARCH;
    const page =
        this.shadowRoot!.querySelector('customize-chrome-wallpaper-search');
    assert(page);
    page.focusOnBackButton();
  }

  protected onCouponsButtonClick_() {
    this.pageHandler_.openChromeWebStoreCategoryPage(
        ChromeWebStoreCategory.kShopping);
  }

  protected onWritingButtonClick_() {
    this.pageHandler_.openChromeWebStoreCollectionPage(
        ChromeWebStoreCollection.kWritingEssentials);
  }

  protected onProductivityButtonClick_() {
    this.pageHandler_.openChromeWebStoreCategoryPage(
        ChromeWebStoreCategory.kWorkflowPlanning);
  }

  protected onChromeWebStoreLinkClick_(e: Event) {
    if ((e.target as HTMLElement).id !== 'chromeWebstoreLink') {
      // Ignore any clicks that are not directly on the <a> element itself. Note
      // that the <a> element is part of a localized string, which is why the
      // listener is added on the parent DOM node.
      return;
    }

    this.pageHandler_.openChromeWebStoreHomePage();
  }

  protected onToolbarCustomizationButtonClick_() {
    this.openToolbarCustomizationPage();
    chrome.metricsPrivate.recordUserAction(
        'Actions.CustomizeToolbarSidePanel.OpenedFromCustomizeChrome');
  }

  private async openToolbarCustomizationPage() {
    this.page_ = CustomizeChromePage.TOOLBAR;
    const page = this.shadowRoot!.querySelector('customize-chrome-toolbar');
    assert(page);
    await this.updateComplete;
    page.focusOnBackButton();
  }

  private pageSupportedOnNonFirstPartyNtps() {
    return this.page_ === CustomizeChromePage.TOOLBAR;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
