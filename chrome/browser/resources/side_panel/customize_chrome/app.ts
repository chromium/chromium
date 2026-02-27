// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_components/help_bubble/new_badge.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './appearance.js';
import './cards.js';
import './categories.js';
import './customize_toolbar/toolbar.js';
import './footer.js';
import './tools.js';
import './shortcuts.js';
import './themes.js';
import './wallpaper_search/wallpaper_search.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {assert, assertNotReached, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AppearanceElement} from './appearance.js';
import type {CategoriesElement} from './categories.js';
import type {BackgroundCollection, ManagementNoticeState} from './customize_chrome.mojom-webui.js';
import {ChromeWebStoreCategory, ChromeWebStoreCollection, CustomizeChromeSection, NewTabPageType} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import type {ThemesElement} from './themes.js';

const SECTION_TO_SELECTOR = {
  [CustomizeChromeSection.kUnspecified]: '',
  [CustomizeChromeSection.kAppearance]: '#appearance',
  [CustomizeChromeSection.kShortcuts]: '#shortcuts',
  [CustomizeChromeSection.kModules]: '#modules',
  [CustomizeChromeSection.kFooter]: '#footer',
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
    overviewPage: HTMLElement,
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
      extensionPolicyEnabled_: {type: Boolean},
      extensionsCardEnabled_: {type: Boolean},
      ntpNextFeaturesEnabled_: {type: Boolean},
      aimPolicyEnabled_: {type: Boolean},
      footerEnabled_: {type: Boolean},
      wallpaperSearchEnabled_: {type: Boolean},
      newTabPageType_: {type: Number},
      showEditTheme_: {type: Boolean},
      showFooter_: {type: Boolean},
      showFooterForManagedBrowser_: {type: Boolean},
    };
  }

  protected accessor page_: CustomizeChromePage = CustomizeChromePage.OVERVIEW;
  protected accessor modulesEnabled_: boolean =
      loadTimeData.getBoolean('modulesEnabled');
  protected accessor selectedCollection_: BackgroundCollection|null = null;
  protected accessor extensionsCardEnabled_: boolean =
      loadTimeData.getBoolean('extensionsCardEnabled');
  protected accessor ntpNextFeaturesEnabled_: boolean =
      loadTimeData.getBoolean('ntpNextFeaturesEnabled');
  protected accessor extensionPolicyEnabled_: boolean = false;
  protected accessor aimPolicyEnabled_: boolean =
      loadTimeData.getBoolean('aimPolicyEnabled');
  protected accessor footerEnabled_: boolean =
      loadTimeData.getBoolean('footerEnabled');
  protected accessor wallpaperSearchEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchEnabled');
  protected accessor newTabPageType_: NewTabPageType =
      NewTabPageType.kFirstPartyWebUI;
  protected accessor showEditTheme_: boolean = true;
  protected accessor showFooter_: boolean = false;
  protected accessor showFooterForManagedBrowser_: boolean = false;

  private listenerIds_: number[] = [];
  private apiProxy_: CustomizeChromeApiProxy =
      CustomizeChromeApiProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.apiProxy_.callbackRouter.scrollToSection.addListener(
          this.onScrollToSection_.bind(this)),
      this.apiProxy_.callbackRouter.attachedTabStateUpdated.addListener(
          this.onAttachedTabStateUpdated_.bind(this)),
      this.apiProxy_.callbackRouter.setThemeEditable.addListener(
          this.onSetThemeEditable_.bind(this)),
      this.apiProxy_.callbackRouter.setFooterSettings.addListener(
          this.onSetFooterSettings_.bind(this)),
    ];

    this.apiProxy_.handler.updateAttachedTabState();
    this.apiProxy_.handler.updateFooterSettings();

    // We wait for load because `scrollIntoView` above requires the page to be
    // laid out.
    window.addEventListener('load', () => {
      this.apiProxy_.handler.updateScrollToSection();
      // Install observer to log extension cards impression.
      const extensionsCardSectionObserver =
          new IntersectionObserver(entries => {
            assert(entries.length >= 1);
            if (entries[0]!.intersectionRatio >= 0.8) {
              extensionsCardSectionObserver.disconnect();
              this.dispatchEvent(
                  new Event('detect-extensions-card-section-impression'));
            }
          }, {
            threshold: 1.0,
          });
      // Start observing if extension cards are scroll into view.
      if (this.shadowRoot.querySelector('#extensions')) {
        extensionsCardSectionObserver.observe(
            this.shadowRoot.querySelector('#extensions')!);
      }
    }, {once: true});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    for (const id of this.listenerIds_) {
      assert(this.apiProxy_.callbackRouter.removeListener(id));
    }
    this.listenerIds_ = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('footerEnabled_') ||
        changedPrivateProperties.has('newTabPageType_') ||
        changedPrivateProperties.has('showFooterForManagedBrowser_') ||
        changedPrivateProperties.has('extensionPolicyEnabled_')) {
      this.showFooter_ = this.computeShowFooter_();
    }
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
    this.registerHelpBubble(
        CHANGE_CHROME_THEME_BUTTON_ELEMENT_ID,
        ['#appearanceElement', '#editThemeButton']);
  }

  private onScrollToSection_(section: CustomizeChromeSection) {
    if (section === CustomizeChromeSection.kWallpaperSearch) {
      this.onWallpaperSearchSelect_();
      return;
    }

    if (section === CustomizeChromeSection.kToolbar) {
      this.openToolbarCustomizationPage();
      chrome.metricsPrivate.recordUserAction(
          'Actions.CustomizeToolbarSidePanel' +
          '.OpenedFromOutsideCustomizeChrome');
      return;
    }

    const selector = SECTION_TO_SELECTOR[section];
    const element = this.shadowRoot.querySelector(selector);
    if (!element) {
      return;
    }

    this.page_ = CustomizeChromePage.OVERVIEW;
    element.scrollIntoView({behavior: 'auto'});
  }


  private onAttachedTabStateUpdated_(newTabPageType: NewTabPageType) {
    if (this.newTabPageType_ === newTabPageType) {
      return;
    }

    this.newTabPageType_ = newTabPageType;

    // Since some pages aren't supported in non first party mode,
    // change the section back to the overview.
    if (!this.isSourceTabFirstPartyNtp_() &&
        !this.pageSupportedOnNonFirstPartyNtps()) {
      this.page_ = CustomizeChromePage.OVERVIEW;
    }
  }

  private onSetThemeEditable_(isThemeEditable: boolean) {
    this.showEditTheme_ = isThemeEditable;
  }

  private onSetFooterSettings_(
      _: boolean, extensionPolicyEnabled: boolean,
      managementNoticeState: ManagementNoticeState) {
    // The footer section should be shown for managed browsers if
    // the management notice is shown or if it is disabled by
    // the user and can be toggled back on.
    this.showFooterForManagedBrowser_ = managementNoticeState.canBeShown;
    this.extensionPolicyEnabled_ = extensionPolicyEnabled;
  }


  protected computeShowFooter_(): boolean {
    return this.footerEnabled_ &&
        ((this.extensionPolicyEnabled_ &&
          this.newTabPageType_ === NewTabPageType.kExtension) ||
         this.showFooterForManagedBrowser_);
  }

  protected isSourceTabFirstPartyNtp_(): boolean {
    return this.newTabPageType_ === NewTabPageType.kFirstPartyWebUI;
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
      case CustomizeChromePage.OVERVIEW:
        assertNotReached();
      default:
        assertNotReachedCase(this.page_);
    }
  }

  protected async onEditThemeClick_() {
    this.page_ = CustomizeChromePage.CATEGORIES;
    await this.updateComplete;
    this.$.categoriesPage.focusOnBackButton();
  }

  protected async onCollectionSelect_(
      event: CustomEvent<BackgroundCollection>) {
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

  protected onWallpaperSearchClick_() {
    this.onWallpaperSearchSelect_();
  }

  protected onWallpaperSearchSelect_() {
    this.page_ = CustomizeChromePage.WALLPAPER_SEARCH;
    const page =
        this.shadowRoot.querySelector('customize-chrome-wallpaper-search');
    assert(page);
    page.focusOnBackButton();
  }

  protected onCouponsButtonClick_() {
    this.apiProxy_.handler.openChromeWebStoreCategoryPage(
        ChromeWebStoreCategory.kShopping);
  }

  protected onWritingButtonClick_() {
    this.apiProxy_.handler.openChromeWebStoreCollectionPage(
        ChromeWebStoreCollection.kWritingEssentials);
  }

  protected onProductivityButtonClick_() {
    this.apiProxy_.handler.openChromeWebStoreCategoryPage(
        ChromeWebStoreCategory.kWorkflowPlanning);
  }

  protected onChromeWebStoreLinkClick_(e: Event) {
    if ((e.target as HTMLElement).id !== 'chromeWebstoreLink') {
      // Ignore any clicks that are not directly on the <a> element itself. Note
      // that the <a> element is part of a localized string, which is why the
      // listener is added on the parent DOM node.
      return;
    }

    this.apiProxy_.handler.openChromeWebStoreHomePage();
  }

  protected onToolbarCustomizationButtonClick_() {
    this.openToolbarCustomizationPage();
    chrome.metricsPrivate.recordUserAction(
        'Actions.CustomizeToolbarSidePanel.OpenedFromCustomizeChrome');
  }

  private async openToolbarCustomizationPage() {
    this.page_ = CustomizeChromePage.TOOLBAR;
    const page = this.shadowRoot.querySelector('customize-chrome-toolbar');
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
