// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects.
 */

import '../../css/wallpaper.css.js';

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {DefaultImageSymbol, kDefaultImageSymbol, kMaximumLocalImagePreviews} from '../../common/constants.js';
import {isNonEmptyArray} from '../../common/utils.js';
import {CollectionsGrid} from '../../untrusted/collections_grid.js';
import {IFrameApi} from '../iframe_api.js';
import {GooglePhotosEnablementState, WallpaperCollection, WallpaperImage, WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getPathOrSymbol} from '../utils.js';

import {getTemplate} from './wallpaper_collections_element.html.js';
import {initializeBackdropData} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

export interface WallpaperCollections {
  $: {collectionsGrid: CollectionsGrid};
}

export class WallpaperCollections extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-collections';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Hidden state of this element. Used to notify of visibility changes. */
      hidden: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      collections_: Array,

      collectionsLoading_: Boolean,

      /**
       * Whether the user is allowed to access Google Photos.
       */
      googlePhotosEnabled_: {
        type: Number,
        observer: 'onGooglePhotosEnabledChanged_',
      },

      /**
       * Contains a mapping of collection id to an array of images.
       */
      images_: Object,

      /**
       * Contains a mapping of collection id to loading boolean.
       */
      imagesLoading_: Object,

      localImages_: Array,

      /**
       * Whether the local image list is currently loading.
       */
      localImagesLoading_: Boolean,

      /**
       * Stores a mapping of local image id to loading status.
       */
      localImageDataLoading_: Object,

      /**
       * Stores a mapping of local image id to thumbnail data.
       */
      localImageData_: Object,

      hasError_: {
        type: Boolean,
        // Call computed functions with their dependencies as arguments so that
        // polymer knows when to re-run the computation.
        computed:
            'computeHasError_(collections_, collectionsLoading_, localImages_, localImagesLoading_)',
      },
    };
  }

  override hidden: boolean;
  private collections_: WallpaperCollection[];
  private collectionsLoading_: boolean;
  private googlePhotosEnabled_: GooglePhotosEnablementState;
  private images_: Record<string, WallpaperImage[]>;
  private imagesLoading_: Record<string, boolean>;
  private localImages_: Array<FilePath|DefaultImageSymbol>|null;
  private localImagesLoading_: boolean;
  private localImageData_: Record<FilePath['path']|DefaultImageSymbol, string>;
  private localImageDataLoading_:
      Record<FilePath['path']|DefaultImageSymbol, boolean>;
  private hasError_: boolean;

  private wallpaperProvider_: WallpaperProviderInterface;
  private didSendLocalImageData_: boolean;


  static get observers() {
    return [
      'onCollectionsChanged_(collections_, collectionsLoading_)',
      'onCollectionImagesChanged_(images_, imagesLoading_)',
      'onLocalImagesChanged_(localImages_, localImagesLoading_)',
      'onLocalImageDataChanged_(localImages_, localImageData_, localImageDataLoading_)',
    ];
  }

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
    this.didSendLocalImageData_ = false;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.wallpaper.backdrop.collections);
    this.watch(
        'collectionsLoading_', state => state.wallpaper.loading.collections);
    this.watch(
        'googlePhotosEnabled_', state => state.wallpaper.googlePhotos.enabled);
    this.watch('images_', state => state.wallpaper.backdrop.images);
    this.watch('imagesLoading_', state => state.wallpaper.loading.images);
    this.watch('localImages_', state => state.wallpaper.local.images);
    this.watch(
        'localImagesLoading_',
        state => state.wallpaper.loading.local.images ||
            state.wallpaper.loading.local.data[kDefaultImageSymbol]);
    this.watch('localImageData_', state => state.wallpaper.local.data);
    this.watch(
        'localImageDataLoading_', state => state.wallpaper.loading.local.data);
    this.updateFromStore();
    initializeBackdropData(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Notify that this element visibility has changed.
   */
  private async onHiddenChanged_(hidden: boolean) {
    if (!hidden) {
      document.title = this.i18n('wallpaperLabel');
    }
    IFrameApi.getInstance().sendVisible(this.$.collectionsGrid, !hidden);
  }

  private computeHasError_(
      collections: WallpaperCollection[], collectionsLoading: boolean,
      localImages: FilePath[], localImagesLoading: boolean): boolean {
    return this.localImagesError_(localImages, localImagesLoading) &&
        this.collectionsError_(collections, collectionsLoading);
  }

  private collectionsError_(
      collections: WallpaperCollection[],
      collectionsLoading: boolean): boolean {
    return !collectionsLoading && !isNonEmptyArray(collections);
  }

  private localImagesError_(
      localImages: FilePath[], localImagesLoading: boolean): boolean {
    return !localImagesLoading && !isNonEmptyArray(localImages);
  }

  private onCollectionsChanged_(
      collections: WallpaperCollection[], collectionsLoading: boolean) {
    // Check whether collections are loaded before sending. Collections could be
    // null/empty array.
    if (!collectionsLoading) {
      IFrameApi.getInstance().sendCollections(
          this.$.collectionsGrid, collections);
    }
  }

  /**
   * Send count of image units in each collection when a new collection is
   * fetched. D/L variants of the same image represent a count of 1.
   */
  private onCollectionImagesChanged_(
      images: Record<string, WallpaperImage[]>,
      imagesLoading: Record<string, boolean>) {
    if (!images || !imagesLoading) {
      return;
    }
    const counts = Object.entries(images)
                       .filter(([collectionId]) => {
                         return imagesLoading[collectionId] === false;
                       })
                       .map(([key, value]) => {
                         // Collection has completed loading. If no images were
                         // retrieved, set count value to null to indicate
                         // failure.
                         if (Array.isArray(value)) {
                           const unitIds = new Set();
                           value.forEach(image => {
                             unitIds.add(image.unitId);
                           });
                           return [key, unitIds.size] as [string, number];
                         } else {
                           return [key, null] as [string, null];
                         }
                       })
                       .reduce((result, [key, value]) => {
                         result[key!] = value;
                         return result;
                       }, {} as Record<string, number|null>);
    IFrameApi.getInstance().sendImageCounts(this.$.collectionsGrid, counts);
  }

  /**
   * Invoked on changes to whether the user is allowed to access Google Photos.
   */
  private onGooglePhotosEnabledChanged_(
      googlePhotosEnabled: WallpaperCollections['googlePhotosEnabled_']) {
    IFrameApi.getInstance().sendGooglePhotosEnabled(
        this.$.collectionsGrid, googlePhotosEnabled);
  }

  /**
   * Send updated local images list.
   */
  private onLocalImagesChanged_(
      localImages: Array<FilePath|DefaultImageSymbol>|null,
      localImagesLoading: boolean) {
    this.didSendLocalImageData_ = false;
    if (!localImagesLoading && Array.isArray(localImages)) {
      IFrameApi.getInstance().sendLocalImages(
          this.$.collectionsGrid, localImages);
    }
  }

  /**
   * Send up to |maximumImageThumbnailsCount| image thumbnails.
   */
  private onLocalImageDataChanged_(
      images: Array<FilePath|DefaultImageSymbol>|null,
      imageData: Record<string|DefaultImageSymbol, string>,
      imageDataLoading: Record<string|DefaultImageSymbol, boolean>) {
    if (!Array.isArray(images) || !imageData || !imageDataLoading ||
        this.didSendLocalImageData_) {
      return;
    }

    const successfullyLoaded: Array<string|DefaultImageSymbol> =
        images.map(image => getPathOrSymbol(image)).filter(key => {
          const doneLoading = imageDataLoading[key] === false;
          const success = !!imageData[key];
          return success && doneLoading;
        });

    function shouldSendImageData() {
      if (!Array.isArray(images)) {
        return false;
      }

      // All images (up to |kMaximumLocalImagePreviews|) have loaded.
      const didLoadMaximum = successfullyLoaded.length >=
          Math.min(kMaximumLocalImagePreviews, images.length);

      return didLoadMaximum ||
          // No more images to load so send now even if some failed.
          images.every(
              image => imageDataLoading[getPathOrSymbol(image)] === false);
    }


    if (shouldSendImageData()) {
      // Also send information about which images failed to load. This is
      // necessary to decide whether to show loading animation or failure svg
      // while updating local images.
      const failures =
          images.map(image => getPathOrSymbol(image))
              .filter((key: string|DefaultImageSymbol) => {
                const doneLoading = imageDataLoading[key] === false;
                const failure = imageData[key] === '';
                return failure && doneLoading;
              })
              .reduce((result, key) => {
                // Empty string means that this image failed to
                // load.
                result[key] = '';
                return result;
              }, {} as Record<FilePath['path']|DefaultImageSymbol, string>);

      const data =
          successfullyLoaded.filter((_, i) => i < kMaximumLocalImagePreviews)
              .reduce((result, key) => {
                result[key] = imageData[key];
                return result;
              }, failures);

      this.didSendLocalImageData_ = true;

      IFrameApi.getInstance().sendLocalImageData(this.$.collectionsGrid, data);
    }
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);
