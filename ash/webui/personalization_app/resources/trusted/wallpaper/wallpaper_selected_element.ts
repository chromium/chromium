// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the currently selected
 * wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../common/icons.js';
import './styles.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {isNonEmptyArray} from '../../common/utils.js';
import {CurrentWallpaper, WallpaperLayout, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {Paths} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getWallpaperLayoutEnum, hasHttpScheme, removeHighResolutionSuffix} from '../utils.js';

import {getDailyRefreshCollectionId, setCurrentWallpaperLayout, setDailyRefreshCollectionId, updateDailyRefreshWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';
import {getTemplate} from './wallpaper_selected_element.html.js';

export class WallpaperSelected extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-selected';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current collection id to display.
       */
      collectionId: String,

      /**
       * The current path of the page.
       */
      path: String,

      image_: {
        type: Object,
        observer: 'onImageChanged_',
      },

      imageTitle_: {
        type: String,
        computed: 'computeImageTitle_(image_, dailyRefreshCollectionId_)',
      },

      imageOtherAttribution_: {
        type: Array,
        computed: 'computeImageOtherAttribution_(image_)',
      },

      dailyRefreshCollectionId_: String,

      isLoading_: Boolean,

      hasError_: {
        type: Boolean,
        computed: 'computeHasError_(image_, isLoading_, error_)',
      },

      showImage_: {
        type: Boolean,
        computed: 'computeShowImage_(image_, isLoading_)',
      },

      showWallpaperOptions_: {
        type: Boolean,
        computed: 'computeShowWallpaperOptions_(image_, path)',
      },

      showCollectionOptions_: {
        type: Boolean,
        computed: 'computeShowCollectionOptions_(path)',
      },

      showRefreshButton_: {
        type: Boolean,
        computed:
            'isDailyRefreshCollectionId_(collectionId,dailyRefreshCollectionId_)',
      },

      dailyRefreshIcon_: {
        type: String,
        computed:
            'computeDailyRefreshIcon_(collectionId,dailyRefreshCollectionId_)',
      },

      ariaPressed_: {
        type: String,
        computed: 'computeAriaPressed_(collectionId,dailyRefreshCollectionId_)',
      },

      fillIcon_: {
        type: String,
        computed: 'computeFillIcon_(image_)',
      },

      centerIcon_: {
        type: String,
        computed: 'computeCenterIcon_(image_)',
      },

      error_: {
        type: String,
        value: null,
      },

      showPreviewButton_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('fullScreenPreviewEnabled');
        }
      }
    };
  }

  collectionId: string;
  path: string;
  private image_: CurrentWallpaper|null;
  private imageTitle_: string;
  private imageOtherAttribution_: string[];
  private dailyRefreshCollectionId_: string|null;
  private isLoading_: boolean;
  private hasError_: boolean;
  private showImage_: boolean;
  private showWallpaperOptions_: boolean;
  private showCollectionOptions_: boolean;
  private showRefreshButton_: boolean;
  private dailyRefreshIcon_: string;
  private ariaPressed_: string;
  private fillIcon_: string;
  private centerIcon_: string;
  private error_: string;
  private showPreviewButton_: boolean;

  private wallpaperProvider_: WallpaperProviderInterface;

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch('error_', state => state.error);
    this.watch('image_', state => state.wallpaper.currentSelected);
    this.watch(
        'isLoading_',
        state => state.wallpaper.loading.setImage > 0 ||
            state.wallpaper.loading.selected ||
            state.wallpaper.loading.refreshWallpaper);
    this.watch(
        'dailyRefreshCollectionId_',
        state => state.wallpaper.dailyRefresh.collectionId);
    this.updateFromStore();
    getDailyRefreshCollectionId(this.wallpaperProvider_, this.getStore());
  }


  /**
   * Return a chrome://image or data:// url to load the image safely. Returns
   * empty string in case |image| is null or invalid.
   */
  private getImageSrc_(image: CurrentWallpaper|null): string {
    if (image && image.url) {
      if (hasHttpScheme(image.url.url)) {
        return `chrome://image?${removeHighResolutionSuffix(image.url.url)}`;
      }
      return image.url.url;
    }
    return '';
  }

  private computeShowImage_(image: CurrentWallpaper|null, loading: boolean):
      boolean {
    // Specifically check === false to avoid undefined case while component is
    // initializing.
    return loading === false && !!image;
  }

  private computeImageTitle_(
      image: CurrentWallpaper|null, dailyRefreshCollectionId: string): string {
    if (!image) {
      return this.i18n('unknownImageAttribution');
    }
    if (isNonEmptyArray(image.attribution)) {
      const title = image.attribution[0];
      return dailyRefreshCollectionId ?
          this.i18n('dailyRefresh') + ': ' + title :
          title;
    } else {
      // Fallback to cached attribution.
      const attribution = this.getLocalStorageAttribution(image.key);
      if (isNonEmptyArray(attribution)) {
        const title = attribution[0];
        return dailyRefreshCollectionId ?
            this.i18n('dailyRefresh') + ': ' + title :
            title;
      }
    }
    return this.i18n('unknownImageAttribution');
  }

  private computeImageOtherAttribution_(image: CurrentWallpaper|
                                        null): string[] {
    if (!image) {
      return [];
    }
    if (isNonEmptyArray(image.attribution)) {
      return image.attribution.slice(1);
    }
    // Fallback to cached attribution.
    const attribution = this.getLocalStorageAttribution(image.key);
    if (isNonEmptyArray(attribution)) {
      return attribution.slice(1);
    }
    return [];
  }

  private computeShowWallpaperOptions_(
      image: CurrentWallpaper|null, path: string): boolean {
    return !!image &&
        ((image.type === WallpaperType.kCustomized &&
              path === Paths.LocalCollection ||
          (image.type === WallpaperType.kGooglePhotos &&
           path === Paths.GooglePhotosCollection)));
  }

  private computeShowCollectionOptions_(path: string): boolean {
    return path === Paths.CollectionImages;
  }

  private getCenterAriaPressed_(image: CurrentWallpaper): string {
    return (!!image && image.layout === WallpaperLayout.kCenter).toString();
  }

  private getFillAriaPressed_(image: CurrentWallpaper): string {
    return (!!image && image.layout === WallpaperLayout.kCenterCropped)
        .toString();
  }

  private computeFillIcon_(image: CurrentWallpaper): string {
    if (!!image && image.layout === WallpaperLayout.kCenterCropped) {
      return 'personalization:checkmark';
    }
    return 'personalization:layout_fill';
  }

  private computeCenterIcon_(image: CurrentWallpaper): string {
    if (!!image && image.layout === WallpaperLayout.kCenter) {
      return 'personalization:checkmark';
    }
    return 'personalization:layout_center';
  }

  private onClickLayoutIcon_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const layout = getWallpaperLayoutEnum(eventTarget.dataset['layout']!);
    setCurrentWallpaperLayout(layout, this.wallpaperProvider_, this.getStore());
  }

  private computeDailyRefreshIcon_(
      collectionId: string, dailyRefreshCollectionId: string): string {
    if (this.isDailyRefreshCollectionId_(
            collectionId, dailyRefreshCollectionId)) {
      return 'personalization:checkmark';
    }
    return 'personalization:change-daily';
  }

  private computeAriaPressed_(
      collectionId: string, dailyRefreshCollectionId: string): string {
    if (this.isDailyRefreshCollectionId_(
            collectionId, dailyRefreshCollectionId)) {
      return 'true';
    }
    return 'false';
  }

  private onClickDailyRefreshToggle_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const collectionId = eventTarget.dataset['collectionId'];
    const dailyRefreshCollectionId =
        eventTarget.dataset['dailyRefreshCollectionId'];
    const isDailyRefreshCollectionId = this.isDailyRefreshCollectionId_(
        collectionId!, dailyRefreshCollectionId!);
    setDailyRefreshCollectionId(
        isDailyRefreshCollectionId ? '' : collectionId!,
        this.wallpaperProvider_, this.getStore());
    // Only refresh the wallpaper if daily refresh is toggled on.
    if (!isDailyRefreshCollectionId) {
      updateDailyRefreshWallpaper(this.wallpaperProvider_, this.getStore());
    }
  }

  /**
   * Determine the current collection view belongs to the collection that is
   * enabled with daily refresh. If true, highlight the toggle and display the
   * refresh button
   */
  private isDailyRefreshCollectionId_(
      collectionId: string, dailyRefreshCollectionId: string): boolean {
    return collectionId === dailyRefreshCollectionId;
  }

  private onClickUpdateDailyRefreshWallpaper_() {
    updateDailyRefreshWallpaper(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Determine whether there is an error in showing selected image. An error
   * happens when there is no previously loaded image and either no new image
   * is being loaded or there is an error from upstream.
   */
  private computeHasError_(
      image: CurrentWallpaper|null, loading: boolean,
      error: string|null): boolean {
    return (!loading || !!error) && !image;
  }

  private getAriaLabel_(image: CurrentWallpaper|null): string {
    if (!image) {
      return this.i18n('currentlySet') + ' ' +
          this.i18n('unknownImageAttribution');
    }
    if (isNonEmptyArray(image.attribution)) {
      return [this.i18n('currentlySet'), ...image.attribution].join(' ');
    }
    // Fallback to cached attribution.
    const attribution = this.getLocalStorageAttribution(image.key);
    if (isNonEmptyArray(attribution)) {
      return [this.i18n('currentlySet'), ...attribution].join(' ');
    }
    return this.i18n('currentlySet') + ' ' +
        this.i18n('unknownImageAttribution');
  }

  /**
   * Returns hidden state of loading placeholder.
   */
  private showPlaceholders_(loading: boolean, showImage: boolean): boolean {
    return loading || !showImage;
  }

  /**
   * Cache the attribution in local storage when image is updated
   * Populate the attribution map in local storage when image is updated
   */
  private async onImageChanged_(
      newImage: CurrentWallpaper|null, oldImage: CurrentWallpaper|null) {
    const attributionMap =
        JSON.parse((window.localStorage['attribution'] || '{}'));
    if (attributionMap.size == 0 ||
        !!newImage && !!oldImage && newImage.key !== oldImage.key) {
      if (newImage) {
        attributionMap[newImage.key] = newImage.attribution;
      }
      if (oldImage) {
        delete attributionMap[oldImage.key];
      }
      window.localStorage['attribution'] = JSON.stringify(attributionMap);
    }
  }

  getLocalStorageAttribution(key: string): string[] {
    const attributionMap =
        JSON.parse((window.localStorage['attribution'] || '{}'));
    const attribution = attributionMap[key];
    if (!attribution) {
      console.warn('Unable to get attribution from local storage.', key);
    }
    return attribution;
  }

  /**
   * Return a container class depending on loading state.
   */
  private getContainerClass_(isLoading: boolean, showImage: boolean): string {
    return this.showPlaceholders_(isLoading, showImage) ? 'loading' : '';
  }
}

customElements.define(WallpaperSelected.is, WallpaperSelected);
