// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the SeaPen recently used
 * wallpapers.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {AnchorAlignment} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {WallpaperGridItemSelectedEvent} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {SeaPenImageId} from './constants.js';
import {RecentSeaPenThumbnailData, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {deleteRecentSeaPenImage, fetchRecentSeaPenData, selectRecentSeaPenImage} from './sea_pen_controller.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {getTemplate} from './sea_pen_recent_wallpapers_element.html.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {isImageDataUrl, isNonEmptyArray, isSeaPenImageId} from './sea_pen_utils.js';

export class SeaPenRecentWallpapersElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-recent-wallpapers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      recentImages_: {
        type: Array,
        observer: 'onRecentImagesChanged_',
      },

      /** Mapping of recent Sea Pen image path to its data. */
      recentImageData_: Object,

      /**
         Mapping of recent Sea Pen image path to data loading status (boolean).
       */
      recentImageDataLoading_: Object,

      recentImagesToDisplay_: {
        type: Array,
        value: [],
      },

      currentShowWallpaperInfoDialog_: {
        type: Number,
        value: null,
      },

      currentSelected_: Number,

      pendingSelected_: Object,
    };
  }

  private recentImages_: SeaPenImageId[]|null;
  private recentImageData_:
      Record<SeaPenImageId, RecentSeaPenThumbnailData|null>;
  private recentImageDataLoading_: Record<SeaPenImageId, boolean>;
  private recentImagesToDisplay_: SeaPenImageId[];
  private currentShowWallpaperInfoDialog_: number|null;
  private currentSelected_: SeaPenImageId|null;
  private pendingSelected_: SeaPenImageId|SeaPenThumbnail|null;

  static get observers() {
    return ['onRecentImageLoaded_(recentImageData_, recentImageDataLoading_)'];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenRecentWallpapersElement['recentImages_']>(
        'recentImages_', state => state.recentImages);
    this.watch<SeaPenRecentWallpapersElement['recentImageData_']>(
        'recentImageData_', state => state.recentImageData);
    this.watch<SeaPenRecentWallpapersElement['recentImageDataLoading_']>(
        'recentImageDataLoading_', state => state.loading.recentImageData);
    this.watch<SeaPenRecentWallpapersElement['currentSelected_']>(
        'currentSelected_', state => state.currentSelected);
    this.watch<SeaPenRecentWallpapersElement['pendingSelected_']>(
        'pendingSelected_', state => state.pendingSelected);
    this.updateFromStore();
    fetchRecentSeaPenData(getSeaPenProvider(), this.getStore());
  }

  /**
   * Sets `recentImagesToDisplay` when a new set of recent Sea Pen images
   * loads.
   */
  private onRecentImagesChanged_(recentImages: SeaPenImageId[]|null) {
    this.recentImagesToDisplay_ = (recentImages || []).filter(id => {
      if (this.recentImageDataLoading_[id] === false) {
        const data = this.recentImageData_[id];
        return data && data.url;
      }
      return true;
    });
  }

  /**
   * Called each time a new recent Sea Pen image data is loaded. Removes images
   * from the list of displayed images if it has failed to load.
   */
  private onRecentImageLoaded_(
      recentImageData: Record<SeaPenImageId, RecentSeaPenThumbnailData|null>,
      recentImageDataLoading: Record<SeaPenImageId, boolean>) {
    if (!recentImageData || !recentImageDataLoading) {
      return;
    }

    // Iterate backwards in case we need to splice to remove from
    // `recentImagesToDisplay` while iterating.
    for (let i = this.recentImagesToDisplay_.length - 1; i >= 0; i--) {
      const id = this.recentImagesToDisplay_[i];
      const data = recentImageData[id];
      const validData = !!data && isImageDataUrl(data.url);
      const failed = id && recentImageDataLoading[id] === false && !validData;
      if (failed) {
        this.recentImagesToDisplay_.splice(i, 1);
      }
    }
  }

  private isRecentImageLoading_(
      recentImage: SeaPenImageId|null,
      recentImageDataLoading: Record<SeaPenImageId, boolean>): boolean {
    if (!recentImage || !recentImageDataLoading) {
      return true;
    }
    // If key is not present, then loading has not yet started. Still show a
    // loading tile in this case.
    return !recentImageDataLoading.hasOwnProperty(recentImage) ||
        recentImageDataLoading[recentImage] === true;
  }

  private getRecentImageUrl_(
      recentImage: SeaPenImageId,
      recentImageData: Record<SeaPenImageId, RecentSeaPenThumbnailData|null>,
      recentImageDataLoading: Record<SeaPenImageId, boolean>): Url|null {
    if (!recentImage ||
        this.isRecentImageLoading_(recentImage, recentImageDataLoading)) {
      return null;
    }
    const data = recentImageData[recentImage];
    if (!data || !isImageDataUrl(data.url)) {
      return {url: ''};
    }
    return data.url;
  }

  private getWallpaperInfoPromptMessage_(
      recentImage: SeaPenImageId,
      recentImageData: Record<SeaPenImageId, RecentSeaPenThumbnailData|null>,
      recentImageDataLoading: Record<SeaPenImageId, boolean>): string|null {
    if (!recentImage ||
        this.isRecentImageLoading_(recentImage, recentImageDataLoading)) {
      return null;
    }

    const data = recentImageData[recentImage];
    if (!data || !data.imageInfo || !data.imageInfo.userVisibleQuery) {
      return null;
    }

    return this.i18n(
        'seaPenAboutDialogPrompt', data.imageInfo.userVisibleQuery.text);
  }

  private getWallpaperInfoDateMessage_(
      recentImage: SeaPenImageId,
      recentImageData: Record<SeaPenImageId, RecentSeaPenThumbnailData|null>,
      recentImageDataLoading: Record<SeaPenImageId, boolean>): string|null {
    if (!recentImage ||
        this.isRecentImageLoading_(recentImage, recentImageDataLoading)) {
      return null;
    }

    const data = recentImageData[recentImage];
    if (!data || !data.imageInfo || !data.imageInfo.creationTime) {
      return null;
    }

    return this.i18n(
        'seaPenAboutDialogDate',
        mojoString16ToString(data.imageInfo.creationTime));
  }

  private getRecentPoweredByGoogleMessage_(): string {
    return window.location.origin === 'chrome://personalization' ?
        this.i18n('seaPenRecentWallpapersHeading') :
        this.i18n('vcBackgroundRecentWallpapersHeading');
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private shouldShowRecentlyUsedWallpapers_(recentImages: SeaPenImageId[]|
                                            null) {
    return isNonEmptyArray(recentImages);
  }

  private isRecentImageSelected_(
      id: SeaPenImageId|null, currentSelected: SeaPenImageId|null,
      pendingSelected: SeaPenImageId|SeaPenThumbnail|null) {
    if (!isSeaPenImageId(id)) {
      return false;
    }

    if (pendingSelected !== null) {
      // User just clicked on a recent image.
      return id === pendingSelected;
    }

    return id === currentSelected;
  }

  private onRecentImageSelected_(event: WallpaperGridItemSelectedEvent&
                                 {model: {image: SeaPenImageId}}) {
    assert(
        isSeaPenImageId(event.model.image),
        'recent Sea Pen image is a positive number');
    selectRecentSeaPenImage(
        event.model.image, getSeaPenProvider(), this.getStore());
  }

  private onClickMenuIcon_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const menuIconContainerRect = targetElement.getBoundingClientRect();
    const config = {
      top: menuIconContainerRect.top -
          8,  // 8px is the padding of .menu-icon-container
      left: menuIconContainerRect.left - menuIconContainerRect.width / 2,
      height: menuIconContainerRect.height,
      width: menuIconContainerRect.width,
      anchorAlignmentX: AnchorAlignment.AFTER_END,
      anchorAlignmentY: AnchorAlignment.BEFORE_START,
    };
    const id = targetElement.dataset['id'];
    if (id !== undefined) {
      const index = parseInt(id, 10);
      const menuElement =
          this.shadowRoot!.querySelectorAll('cr-action-menu')[index];
      menuElement!.showAtPosition(config);
    }
  }

  private onClickMoreLikeThis_() {
    // TODO(b/304581483): make "More like this" button functional.
  }

  private onClickDeleteWallpaper_(event: Event&
                                  {model: {image: SeaPenImageId}}) {
    // TODO (b/315069374): confirm if currently set Sea Pen wallpaper can be
    // removed.
    assert(
        isSeaPenImageId(event.model.image),
        'selected Sea Pen image is a positive number');
    deleteRecentSeaPenImage(
        event.model.image, getSeaPenProvider(), this.getStore());
    this.closeAllActionMenus_();
  }

  private onClickWallpaperInfo_(e: Event) {
    const eventTarget = e.currentTarget as HTMLElement;
    const id = eventTarget.dataset['id'];
    if (id !== undefined) {
      this.currentShowWallpaperInfoDialog_ = parseInt(id, 10);
    }
    this.closeAllActionMenus_();
  }

  private closeAllActionMenus_() {
    const menuElements = this.shadowRoot!.querySelectorAll('cr-action-menu');
    menuElements.forEach(menuElement => {
      menuElement.close();
    });
  }

  private shouldShowWallpaperInfoButton_(
      recentImage: SeaPenImageId,
      recentImageData: Record<SeaPenImageId, RecentSeaPenThumbnailData|null>,
      recentImageDataLoading: Record<SeaPenImageId, boolean>): boolean {
    if (!recentImage ||
        this.isRecentImageLoading_(recentImage, recentImageDataLoading)) {
      return false;
    }

    const data = recentImageData[recentImage];
    return !!data && !!data.imageInfo && !!data.imageInfo.creationTime &&
        !!data.imageInfo.userVisibleQuery;
  }

  private shouldShowWallpaperInfoDialog_(
      i: number, currentShowWallpaperInfoDialog: number|null): boolean {
    return currentShowWallpaperInfoDialog === i;
  }

  private onCloseDialog_() {
    this.currentShowWallpaperInfoDialog_ = null;
  }
}

customElements.define(
    SeaPenRecentWallpapersElement.is, SeaPenRecentWallpapersElement);
