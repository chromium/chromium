// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the SeaPen recently used
 * wallpapers.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '../../../css/common.css.js';
import '../../../css/wallpaper.css.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {CurrentWallpaper} from '../../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../../personalization_store.js';
import {isImageDataUrl, isNonEmptyArray} from '../../utils.js';
import {DisplayableImage} from '../constants.js';
import {isFilePath, isImageAMatchForKey, isImageEqualToSelected} from '../utils.js';
import {WallpaperGridItemSelectedEvent} from '../wallpaper_grid_item_element.js';

import {RecentSeaPenData} from './constants.js';
import {fetchRecentSeaPenData, selectRecentSeaPenImage} from './sea_pen_controller.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {getTemplate} from './sea_pen_recent_wallpapers_element.html.js';

export class SeaPenRecentWallpapersElement extends WithPersonalizationStore {
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

      currentSelected_: Object,

      pendingSelected_: Object,
    };
  }

  private recentImages_: FilePath[]|null;
  private recentImageData_: Record<FilePath['path'], RecentSeaPenData>;
  private recentImageDataLoading_: Record<FilePath['path'], boolean>;
  private recentImagesToDisplay_: FilePath[];
  private currentShowWallpaperInfoDialog_: number|null;
  private currentSelected_: CurrentWallpaper|null;
  private pendingSelected_: DisplayableImage|null;

  static get observers() {
    return ['onRecentImageLoaded_(recentImageData_, recentImageDataLoading_)'];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenRecentWallpapersElement['recentImages_']>(
        'recentImages_', state => state.wallpaper.seaPen.recentImages);
    this.watch<SeaPenRecentWallpapersElement['recentImageData_']>(
        'recentImageData_', state => state.wallpaper.seaPen.recentImageData);
    this.watch<SeaPenRecentWallpapersElement['recentImageDataLoading_']>(
        'recentImageDataLoading_',
        state => state.wallpaper.loading.seaPen.recentImageData);
    this.watch<SeaPenRecentWallpapersElement['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<SeaPenRecentWallpapersElement['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.updateFromStore();
    // TODO(b/304576846): also refetch sea pen data when adding and deleting
    // image.
    fetchRecentSeaPenData(getSeaPenProvider(), this.getStore());
  }

  /**
   * Sets `recentImagesToDisplay` when a new set of recent Sea Pen images
   * loads.
   */
  private onRecentImagesChanged_(recentImages: FilePath[]|null) {
    this.recentImagesToDisplay_ = (recentImages || []).filter(image => {
      if (isFilePath(image) &&
          this.recentImageDataLoading_[image.path] === false) {
        const data = this.recentImageData_[image.path];
        return data && data.queryInfo && data.url;
      }
      return true;
    });
  }

  /**
   * Called each time a new recent Sea Pen image data is loaded. Removes images
   * from the list of displayed images if it has failed to load.
   */
  private onRecentImageLoaded_(
      recentImageData: Record<FilePath['path'], RecentSeaPenData>,
      recentImageDataLoading: Record<FilePath['path'], boolean>) {
    if (!recentImageData || !recentImageDataLoading) {
      return;
    }

    // Iterate backwards in case we need to splice to remove from
    // `recentImagesToDisplay` while iterating.
    for (let i = this.recentImagesToDisplay_.length - 1; i >= 0; i--) {
      const image = this.recentImagesToDisplay_[i];
      const failed = image && recentImageDataLoading[image.path] === false &&
          !isImageDataUrl(recentImageData[image.path].url);
      if (failed) {
        this.recentImagesToDisplay_.splice(i, 1);
      }
    }
  }

  private isRecentImageLoading_(
      recentImage: FilePath|null,
      recentImageDataLoading: Record<FilePath['path'], boolean>): boolean {
    if (!recentImage || !recentImageDataLoading) {
      return true;
    }
    // If key is not present, then loading has not yet started. Still show a
    // loading tile in this case.
    return !recentImageDataLoading.hasOwnProperty(recentImage.path) ||
        recentImageDataLoading[recentImage.path] === true;
  }

  private getRecentImageUrl_(
      recentImage: FilePath,
      recentImageData: Record<FilePath['path'], RecentSeaPenData>,
      recentImageDataLoading: Record<FilePath['path'], boolean>): Url|null {
    if (!recentImage ||
        this.isRecentImageLoading_(recentImage, recentImageDataLoading)) {
      return null;
    }
    const data = recentImageData[recentImage.path];
    if (!data || !isImageDataUrl(data.url)) {
      return {url: ''};
    }
    return data.url;
  }

  private getWallpaperInfoMessage_(
      recentImage: FilePath,
      recentImageData: Record<FilePath['path'], RecentSeaPenData>,
      recentImageDataLoading: Record<FilePath['path'], boolean>): string|null {
    if (!recentImage ||
        this.isRecentImageLoading_(recentImage, recentImageDataLoading)) {
      return null;
    }
    return recentImageData[recentImage.path].queryInfo;
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private shouldShowRecentlyUsedWallpapers_(recentImages: FilePath[]|null) {
    return isNonEmptyArray(recentImages);
  }

  private isRecentImageSelected_(
      image: FilePath, currentSelected: CurrentWallpaper|null,
      pendingSelected: DisplayableImage|null) {
    if (!isFilePath(image)) {
      return false;
    }
    return (isFilePath(pendingSelected) &&
            isImageAMatchForKey(image, pendingSelected.path)) ||
        (!pendingSelected && !!currentSelected &&
         isImageEqualToSelected(image, currentSelected));
  }

  private onRecentImageSelected_(event: WallpaperGridItemSelectedEvent&
                                 {model: {image: FilePath}}) {
    assert(
        isFilePath(event.model.image), 'recent Sea Pen image is a file path');
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
          this.shadowRoot!.querySelectorAll('cr-action-menu')![index];
      menuElement!.showAtPosition(config);
    }
  }

  private onClickMoreLikeThis_() {
    // TODO(b/304581483): make "More like this" button functional.
  }

  private onClickDeleteWallpaper_() {
    // TODO(b/304592162): delete the selected wallpaper.
  }

  private onClickWallpaperInfo_(e: Event) {
    const eventTarget = e.currentTarget as HTMLElement;
    const id = eventTarget.dataset['id'];
    if (id !== undefined) {
      this.currentShowWallpaperInfoDialog_ = parseInt(id, 10);
      this.closeAllActionMenus_();
    }
  }

  private closeAllActionMenus_() {
    const menuElements = this.shadowRoot!.querySelectorAll('cr-action-menu');
    menuElements.forEach(menuElement => {
      menuElement.close();
    });
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
