// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './check_mark_wrapper.js';
import './strings.m.js';
import './wallpaper_search/wallpaper_search_tile.js';

import type {SpHeadingElement} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrA11yAnnouncerElement} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './categories.css.js';
import {getHtml} from './categories.html.js';
import {CustomizeChromeAction, NtpImageType, recordCustomizeChromeAction, recordCustomizeChromeImageError} from './common.js';
import type {BackgroundCollection, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
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

const CategoriesElementBase = I18nMixinLit(HelpBubbleMixinLit(CrLitElement));

export interface CategoriesElement {
  $: {
    chromeWebStoreTile: HTMLElement,
    classicChromeTile: HTMLElement,
    heading: SpHeadingElement,
    uploadImageTile: HTMLElement,
  };
}

export class CategoriesElement extends CategoriesElementBase {
  static get is() {
    return 'customize-chrome-categories';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      collections_: {type: Array},
      theme_: {type: Object},
      selectedCategory_: {type: Object},
      isClassicChromeSelected_: {type: Boolean},
      isLocalImageSelected_: {type: Boolean},
      isWallpaperSearchSelected_: {type: Boolean},
      wallpaperSearchEnabled_: {type: Boolean},
    };
  }

  protected collections_: BackgroundCollection[] = [];
  private selectedCategory_: SelectedCategory = {type: CategoryType.NONE};
  private theme_: Theme|null = null;
  protected isClassicChromeSelected_: boolean = false;
  protected isLocalImageSelected_: boolean = false;
  protected isWallpaperSearchSelected_: boolean = false;
  protected wallpaperSearchEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchEnabled');
  protected imageErrorDetectionEnabled_: boolean =
      loadTimeData.getBoolean('imageErrorDetectionEnabled');

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

  override firstUpdated() {
    this.registerHelpBubble(
        CHANGE_CHROME_THEME_CLASSIC_ELEMENT_ID, '#classicChromeTile');
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    this.selectedCategory_ = this.computeSelectedCategory_();
    this.isClassicChromeSelected_ =
        this.selectedCategory_.type === CategoryType.CLASSIC;
    this.isLocalImageSelected_ =
        this.selectedCategory_.type === CategoryType.LOCAL;
    this.isWallpaperSearchSelected_ =
        this.selectedCategory_.type === CategoryType.WALLPAPER_SEARCH;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('collections_') &&
        this.collections_.length > 0) {
      this.onCollectionsRendered_();
    }
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private onCollectionsRendered_() {
    const collections = this.shadowRoot!.querySelectorAll('.collection');
    if (collections.length >= 5) {
      this.registerHelpBubble(
          CHROME_THEME_COLLECTION_ELEMENT_ID, collections[4]!);
    }
  }

  protected shouldShowCollection_(itemLoaded: boolean) {
    return !this.imageErrorDetectionEnabled_ || itemLoaded;
  }

  protected onPreviewImageLoad_(e: Event) {
    if (this.imageErrorDetectionEnabled_) {
      const index = Number((e.currentTarget as HTMLElement).dataset['index']);
      assert(this.collections_[index]);
      this.collections_[index].imageVerified = true;
      this.requestUpdate();
    }
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

  protected onPreviewImageError_(e: Event) {
    if (!this.imageErrorDetectionEnabled_) {
      return;
    }
    recordCustomizeChromeImageError(NtpImageType.COLLECTIONS);
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    assert(this.collections_[index]);
    this.pageHandler_
        .getReplacementCollectionPreviewImage(this.collections_[index].id)
        .then(({previewImageUrl}) => {
          if (previewImageUrl) {
            this.collections_[index]!.previewImageUrl = previewImageUrl;
            this.requestUpdate();
          }
        });
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

  protected isCollectionSelected_(id: string) {
    return this.selectedCategory_.type === CategoryType.COLLECTION &&
        this.selectedCategory_.collectionId === id;
  }

  protected onClassicChromeClick_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_DEFAULT_CHROME_SELECTED);
    this.pageHandler_.setDefaultColor();
    this.pageHandler_.removeBackgroundImage();
  }

  protected onWallpaperSearchClick_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_WALLPAPER_SEARCH_SELECTED);
    this.dispatchEvent(new Event('wallpaper-search-select'));
  }

  protected async onUploadImageClick_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_UPLOAD_IMAGE_SELECTED);
    chrome.metricsPrivate.recordUserAction(
        'NTPRicherPicker.Backgrounds.UploadClicked');
    const {success} = await this.pageHandler_.chooseLocalCustomBackground();
    if (success) {
      const announcer = getAnnouncerInstance() as CrA11yAnnouncerElement;
      announcer.announce(this.i18n('updatedToUploadedImage'));
      this.dispatchEvent(new Event('local-image-upload'));
    }
  }

  protected onCollectionClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    recordCustomizeChromeAction(
        CustomizeChromeAction.CATEGORIES_FIRST_PARTY_COLLECTION_SELECTED);
    this.dispatchEvent(new CustomEvent<BackgroundCollection>(
        'collection-select', {detail: this.collections_[index]}));
  }

  protected onChromeWebStoreClick_() {
    this.pageHandler_.openChromeWebStore();
  }

  protected onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-categories': CategoriesElement;
  }
}

customElements.define(CategoriesElement.is, CategoriesElement);
