// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './check_mark_wrapper.js';
import './strings.m.js';
import './wallpaper_search/wallpaper_search_tile.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {HelpBubbleMixin, HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './categories.html.js';
import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import {BackgroundCollection, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {WindowProxy} from './window_proxy.js';

export enum CategoryType {
  NONE,
  CLASSIC,
  LOCAL,
  COLOR,
  COLLECTION,
  WALLPAPER_SEARCH,
}

export const CHROME_THEME_COLLECTION_ELEMENT_ID =
    'CustomizeChromeUI::kChromeThemeCollectionElementId';
export const CHANGE_CHROME_THEME_CLASSIC_ELEMENT_ID =
    'CustomizeChromeUI::kChangeChromeThemeClassicElementId';

export interface SelectedCategory {
  type: CategoryType;
  collectionId?: string;
}

const CategoriesElementBase = HelpBubbleMixin(PolymerElement) as
    {new (): PolymerElement & HelpBubbleMixinInterface};

export interface CategoriesElement {
  $: {
    chromeWebStoreTile: HTMLElement,
    classicChromeTile: HTMLElement,
    heading: SpHeading,
    uploadImageTile: HTMLElement,
  };
}

export class CategoriesElement extends CategoriesElementBase {
  static get is() {
    return 'customize-chrome-categories';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      chromeRefresh2023Enabled_: {
        type: Boolean,
        value: () =>
            document.documentElement.hasAttribute('chrome-refresh-2023'),
      },
      collections_: Array,
      theme_: Object,
      selectedCategory_: {
        type: Object,
        computed: 'computeSelectedCategory_(theme_, collections_)',
      },
      isClassicChromeSelected_: {
        type: Boolean,
        computed: 'computeIsClassicChromeSelected_(selectedCategory_)',
      },
      isLocalImageSelected_: {
        type: Boolean,
        computed: 'computeIsLocalImageSelected_(selectedCategory_)',
      },
      isWallpaperSearchSelected_: {
        type: Boolean,
        computed: 'computeIsWallpaperSearchSelected_(selectedCategory_)',
      },
      isChromeColorsSelected_: {
        type: Boolean,
        computed: 'computeIsChromeColorsSelected_(selectedCategory_)',
      },
      wallpaperSearchEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('wallpaperSearchEnabled'),
      },
    };
  }

  private collections_: BackgroundCollection[];
  private selectedCategory_: SelectedCategory;
  private theme_: Theme;

  private pageHandler_: CustomizeChromePageHandlerInterface;
  private previewImageLoadStartEpoch_: number;
  private setThemeListenerId_: number|null = null;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.previewImageLoadStartEpoch_ = WindowProxy.getInstance().now();
    this.pageHandler_.getBackgroundCollections().then(({collections}) => {
      this.collections_ = collections;
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.setTheme.addListener((theme: Theme) => {
              this.theme_ = theme;
            });
    this.pageHandler_.updateTheme();
    FocusOutlineManager.forDocument(document);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.setThemeListenerId_!);
  }

  override ready() {
    super.ready();
    this.registerHelpBubble(
        CHANGE_CHROME_THEME_CLASSIC_ELEMENT_ID, '#classicChromeTile');
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private onCollectionsRendered_() {
    const collections = this.root!.querySelectorAll('.collection');
    if (collections.length >= 5) {
      this.registerHelpBubble(
          CHROME_THEME_COLLECTION_ELEMENT_ID, collections[4]);
    }
  }

  private onPreviewImageLoad_() {
    chrome.metricsPrivate.recordValue(
        {
          metricName: 'NewTabPage.Images.ShownTime.CollectionPreviewImage',
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
          min: 1,
          max: 60000,  // 60 seconds.
          buckets: 100,
        },
        Math.floor(
            WindowProxy.getInstance().now() -
            this.previewImageLoadStartEpoch_));
  }

  private computeSelectedCategory_() {
    if (!this.theme_ || this.theme_.thirdPartyThemeInfo) {
      return {type: CategoryType.NONE};
    }
    if (!this.theme_.backgroundImage) {
      if (!this.theme_.foregroundColor) {
        return {type: CategoryType.CLASSIC};
      }
      return {type: CategoryType.COLOR};
    }
    if (this.theme_.backgroundImage.isUploadedImage) {
      return this.theme_.backgroundImage.localBackgroundId ?
          {type: CategoryType.WALLPAPER_SEARCH} :
          {type: CategoryType.LOCAL};
    }
    if (this.theme_.backgroundImage.collectionId) {
      return {
        type: CategoryType.COLLECTION,
        collectionId: this.theme_.backgroundImage.collectionId,
      };
    }
    return {type: CategoryType.NONE};
  }

  private computeIsClassicChromeSelected_() {
    return this.selectedCategory_.type === CategoryType.CLASSIC;
  }

  private computeIsLocalImageSelected_() {
    return this.selectedCategory_.type === CategoryType.LOCAL;
  }

  private computeIsWallpaperSearchSelected_() {
    return this.selectedCategory_.type === CategoryType.WALLPAPER_SEARCH;
  }

  private computeIsChromeColorsSelected_() {
    return this.selectedCategory_.type === CategoryType.COLOR;
  }

  private isCollectionSelected_(id: string) {
    return this.selectedCategory_.type === CategoryType.COLLECTION &&
        this.selectedCategory_.collectionId === id;
  }

  private boolToString_(value: boolean): string {
    return value ? 'true' : 'false';
  }

  private getCollectionCheckedStatus_(id: string): string {
    return this.boolToString_(this.isCollectionSelected_(id));
  }

  private onClassicChromeClick_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_DEFAULT_CHROME_SELECTED);
    this.pageHandler_.setDefaultColor();
    this.pageHandler_.removeBackgroundImage();
  }

  private onWallpaperSearchClick_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_WALLPAPER_SEARCH_SELECTED);
    this.dispatchEvent(new Event('wallpaper-search-select'));
  }

  private async onUploadImageClick_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_UPLOAD_IMAGE_SELECTED);
    chrome.metricsPrivate.recordUserAction(
        'NTPRicherPicker.Backgrounds.UploadClicked');
    const {success} = await this.pageHandler_.chooseLocalCustomBackground();
    if (success) {
      this.dispatchEvent(new Event('local-image-upload'));
    }
  }

  private async onChromeColorsClick_() {
    this.dispatchEvent(new Event('chrome-colors-select'));
  }

  private onCollectionClick_(e: DomRepeatEvent<BackgroundCollection>) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_FIRST_PARTY_COLLECTION_SELECTED);
    this.dispatchEvent(new CustomEvent<BackgroundCollection>(
        'collection-select', {detail: e.model.item}));
  }

  private onChromeWebStoreClick_() {
    this.pageHandler_.openChromeWebStore();
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-categories': CategoriesElement;
  }
}

customElements.define(CategoriesElement.is, CategoriesElement);
