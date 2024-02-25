// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../../common/icons.html.js';

import {WallpaperGridItemSelectedEvent} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {isImageDataUrl, isNonEmptyFilePath} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CurrentWallpaper, WallpaperProviderInterface, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {DefaultImageSymbol, DisplayableImage, kDefaultImageSymbol} from './constants.js';
import {getTemplate} from './local_images_element.html.js';
import {getPathOrSymbol, isDefaultImage} from './utils.js';
import {selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';


export class LocalImagesElement extends WithPersonalizationStore {
  static get is() {
    return 'local-images';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      images_: {
        type: Array,
        observer: 'onImagesChanged_',
      },

      /** Mapping of local image path to data url. */
      imageData_: Object,

      /** Mapping of local image path to boolean. */
      imageDataLoading_: Object,

      currentSelected_: Object,

      /** The pending selected image. */
      pendingSelected_: Object,

      imagesToDisplay_: {
        type: Array,
        value: [],
      },
    };
  }

  static get observers() {
    return ['onImageLoaded_(imageData_, imageDataLoading_)'];
  }

  private wallpaperProvider_: WallpaperProviderInterface;
  private images_: Array<FilePath|DefaultImageSymbol>|null;
  private imageData_: Record<FilePath['path']|DefaultImageSymbol, Url>;
  private imageDataLoading_:
      Record<FilePath['path']|DefaultImageSymbol, boolean>;
  private currentSelected_: CurrentWallpaper|null;
  private pendingSelected_: DisplayableImage|null;
  private imagesToDisplay_: Array<FilePath|DefaultImageSymbol>;

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  override ready() {
    super.ready();
    afterNextRender(this, () => {
      this.shadowRoot!.getElementById('main')!.focus();
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<LocalImagesElement['images_']>(
        'images_', state => state.wallpaper.local.images);
    this.watch<LocalImagesElement['imageData_']>(
        'imageData_', state => state.wallpaper.local.data);
    this.watch<LocalImagesElement['imageDataLoading_']>(
        'imageDataLoading_', state => state.wallpaper.loading.local.data);
    this.watch<LocalImagesElement['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<LocalImagesElement['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.updateFromStore();
  }

  /** Sets |imagesToDisplay| when a new set of local images loads. */
  private onImagesChanged_(images: LocalImagesElement['images_']) {
    this.imagesToDisplay_ = (images || []).filter(image => {
      const key = getPathOrSymbol(image);
      if (this.imageDataLoading_[key] === false) {
        return isImageDataUrl(this.imageData_[key]);
      }
      return true;
    });
  }

  /**
   * Called each time a new image thumbnail is loaded. Removes images
   * from the list of displayed images if it has failed to load.
   */
  private onImageLoaded_(
      imageData: LocalImagesElement['imageData_'],
      imageDataLoading: LocalImagesElement['imageDataLoading_']) {
    if (!imageData || !imageDataLoading) {
      return;
    }
    // Iterate backwards in case we need to splice to remove from
    // |imagesToDisplay| while iterating.
    for (let i = this.imagesToDisplay_.length - 1; i >= 0; i--) {
      const image = this.imagesToDisplay_[i];
      const key = getPathOrSymbol(image);
      const failed =
          imageDataLoading[key] === false && !isImageDataUrl(imageData[key]);
      if (failed) {
        this.splice('imagesToDisplay_', i, 1);
      }
    }
  }

  private isImageSelected_(
      image: FilePath|DefaultImageSymbol|null,
      currentSelected: LocalImagesElement['currentSelected_'],
      pendingSelected: LocalImagesElement['pendingSelected_']): boolean {
    if (!image || (!currentSelected && !pendingSelected)) {
      return false;
    }
    if (isDefaultImage(image)) {
      return (
          (isDefaultImage(pendingSelected)) ||
          (!pendingSelected && !!currentSelected &&
           currentSelected.type === WallpaperType.kDefault));
    }
    return (
        isNonEmptyFilePath(pendingSelected) &&
            image.path === pendingSelected.path ||
        !!currentSelected && image.path === currentSelected.key &&
            !pendingSelected);
  }

  private getAriaLabel_(
      image: FilePath|DefaultImageSymbol|null,
      imageDataLoading: LocalImagesElement['imageDataLoading_']): string {
    if (this.isImageLoading_(image, imageDataLoading)) {
      return this.i18n('ariaLabelLoading');
    }
    if (isDefaultImage(image)) {
      return this.i18n('defaultWallpaper');
    }
    if (!isNonEmptyFilePath(image)) {
      return '';
    }
    const path = image.path;
    return path.substring(path.lastIndexOf('/') + 1);
  }

  private isImageLoading_(
      image: FilePath|DefaultImageSymbol|null,
      imageDataLoading: LocalImagesElement['imageDataLoading_']): boolean {
    if (!image || !imageDataLoading) {
      return true;
    }
    const key = getPathOrSymbol(image);
    // If key is not present, then loading has not yet started. Still show a
    // loading tile in this case.
    return !imageDataLoading.hasOwnProperty(key) ||
        imageDataLoading[key] === true;
  }

  private getImageData_(
      image: FilePath|DefaultImageSymbol|null,
      imageData: LocalImagesElement['imageData_'],
      imageDataLoading: LocalImagesElement['imageDataLoading_']): Url|null {
    if (!image || this.isImageLoading_(image, imageDataLoading)) {
      return null;
    }
    const data = imageData[getPathOrSymbol(image)];
    // Return a "fail" url that will not load.
    if (!isImageDataUrl(data)) {
      return {url: ''};
    }
    return data;
  }

  private getImageDataId_(image: FilePath|DefaultImageSymbol|null): string {
    if (!image) {
      return '';
    }
    return isNonEmptyFilePath(image) ? image.path : image.toString();
  }

  private onImageSelected_(event: WallpaperGridItemSelectedEvent&
                           {model: {item: FilePath | DefaultImageSymbol}}) {
    assert(
        event.model.item === kDefaultImageSymbol ||
            isNonEmptyFilePath(event.model.item),
        'local image is a file path or default image');
    selectWallpaper(event.model.item, this.wallpaperProvider_, this.getStore());
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }
}

customElements.define(LocalImagesElement.is, LocalImagesElement);
