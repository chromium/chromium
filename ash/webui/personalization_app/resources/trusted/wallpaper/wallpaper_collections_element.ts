// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects.
 */

import './styles.js';

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {kMaximumGooglePhotosPreviews, kMaximumLocalImagePreviews} from '../../common/constants.js';
import {isNonEmptyArray, isNullOrArray, isNullOrNumber, promisifyOnload} from '../../common/utils.js';
import {sendCollections, sendGooglePhotosCount, sendGooglePhotosPhotos, sendImageCounts, sendLocalImageData, sendLocalImages, sendVisible} from '../iframe_api.js';
import {WallpaperCollection, WallpaperImage, WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {initializeBackdropData} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

let sendCollectionsFunction = sendCollections;
let sendGooglePhotosCountFunction = sendGooglePhotosCount;
let sendGooglePhotosPhotosFunction = sendGooglePhotosPhotos;
let sendImageCountsFunction = sendImageCounts;
let sendLocalImagesFunction = sendLocalImages;
let sendLocalImageDataFunction = sendLocalImageData;

/**
 * Mock out the iframe api functions for testing. Return promises that are
 * resolved when the function is called by |WallpaperCollectionsElement|.
 */
interface PromisifyResult {
  sendCollections: Promise<unknown>;
  sendGooglePhotosCount: Promise<unknown>;
  sendGooglePhotosPhotos: Promise<unknown>;
  sendImageCounts: Promise<unknown>;
  sendLocalImages: Promise<unknown>;
  sendLocalImageData: Promise<unknown>;
}

export function promisifyIframeFunctionsForTesting(): PromisifyResult {
  const resolvers = {} as
      {[key in keyof PromisifyResult]: (_: unknown) => void};
  const promises = ([
                     'sendCollections',
                     'sendGooglePhotosCount',
                     'sendGooglePhotosPhotos',
                     'sendImageCounts',
                     'sendLocalImages',
                     'sendLocalImageData',
                   ] as (keyof PromisifyResult)[])
                       .reduce((result, next) => {
                         result[next] =
                             new Promise(resolve => resolvers[next] = resolve);
                         return result;
                       }, {} as PromisifyResult);
  sendCollectionsFunction = (...args) => resolvers['sendCollections'](args);
  sendGooglePhotosCountFunction = (...args) =>
      resolvers['sendGooglePhotosCount'](args);
  sendGooglePhotosPhotosFunction = (...args) =>
      resolvers['sendGooglePhotosPhotos'](args);
  sendImageCountsFunction = (...args) => resolvers['sendImageCounts'](args);
  sendLocalImagesFunction = (...args) => resolvers['sendLocalImages'](args);
  sendLocalImageDataFunction = (...args) =>
      resolvers['sendLocalImageData'](args);
  return promises;
}

export class WallpaperCollections extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-collections';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Hidden state of this element. Used to notify iframe of visibility
       * changes.
       */
      hidden: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      collections_: Array,

      collectionsLoading_: Boolean,

      /**
       * The list of Google Photos photos.
       */
      googlePhotos_: Array,

      /**
       * Whether the list of Google Photos photos is currently loading.
       */
      googlePhotosLoading_: Boolean,

      /**
       * The count of Google Photos photos.
       */
      googlePhotosCount_: Number,

      /**
       * Whether the count of Google Photos photos is currently loading.
       */
      googlePhotosCountLoading_: Boolean,

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

  hidden: boolean;
  private collections_: WallpaperCollection[];
  private collectionsLoading_: boolean;
  private googlePhotos_: unknown[]|null;
  private googlePhotosLoading_: boolean;
  private googlePhotosCount_: number|null;
  private googlePhotosCountLoading_: boolean;
  private images_: Record<string, WallpaperImage[]>;
  private imagesLoading_: Record<string, boolean>;
  private localImages_: FilePath[];
  private localImagesLoading_: boolean;
  private localImageData_: Record<string, string>;
  private localImageDataLoading_: Record<string, boolean>;
  private hasError_: boolean;

  private wallpaperProvider_: WallpaperProviderInterface;
  private iframePromise_: Promise<HTMLIFrameElement>;
  private didSendLocalImageData_: boolean;


  static get observers() {
    return [
      'onCollectionsChanged_(collections_, collectionsLoading_)',
      'onCollectionImagesChanged_(images_, imagesLoading_)',
      'onGooglePhotosChanged_(googlePhotos_, googlePhotosLoading_)',
      'onGooglePhotosCountChanged_(googlePhotosCount_, googlePhotosCountLoading_)',
      'onLocalImagesChanged_(localImages_, localImagesLoading_)',
      'onLocalImageDataChanged_(localImages_, localImageData_, localImageDataLoading_)',
    ];
  }

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
    this.iframePromise_ =
        promisifyOnload(this, 'collections-iframe', afterNextRender) as
        Promise<HTMLIFrameElement>;
    this.didSendLocalImageData_ = false;
  }

  connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.wallpaper.backdrop.collections);
    this.watch(
        'collectionsLoading_', state => state.wallpaper.loading.collections);
    this.watch('googlePhotos_', state => state.wallpaper.googlePhotos.photos);
    this.watch(
        'googlePhotosLoading_',
        state => state.wallpaper.loading.googlePhotos.photos);
    this.watch(
        'googlePhotosCount_', state => state.wallpaper.googlePhotos.count);
    this.watch(
        'googlePhotosCountLoading_',
        state => state.wallpaper.loading.googlePhotos.count);
    this.watch('images_', state => state.wallpaper.backdrop.images);
    this.watch('imagesLoading_', state => state.wallpaper.loading.images);
    this.watch('localImages_', state => state.wallpaper.local.images);
    this.watch(
        'localImagesLoading_', state => state.wallpaper.loading.local.images);
    this.watch('localImageData_', state => state.wallpaper.local.data);
    this.watch(
        'localImageDataLoading_', state => state.wallpaper.loading.local.data);
    this.updateFromStore();
    initializeBackdropData(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Notify iframe that this element visibility has changed.
   */
  private async onHiddenChanged_(hidden: boolean) {
    if (!hidden) {
      document.title = this.i18n('title');
    }
    const iframe = await this.iframePromise_;
    sendVisible(iframe.contentWindow!, !hidden);
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

  private async onCollectionsChanged_(
      collections: WallpaperCollection[], collectionsLoading: boolean) {
    // Check whether collections are loaded before sending to
    // the iframe. Collections could be null/empty array.
    if (!collectionsLoading) {
      const iframe = await this.iframePromise_;
      sendCollectionsFunction(iframe.contentWindow!, collections);
    }
  }

  /**
   * Send count of image units in each collection when a new collection is
   * fetched. D/L variants of the same image represent a count of 1.
   */
  private async onCollectionImagesChanged_(
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
    const iframe = await this.iframePromise_;
    sendImageCountsFunction(iframe.contentWindow!, counts);
  }

  /** Invoked on changes to the list of Google Photos photos. */
  private async onGooglePhotosChanged_(
      googlePhotos: Url[]|null, googlePhotosLoading: boolean) {
    if (googlePhotosLoading || !isNullOrArray(googlePhotos)) {
      return;
    }
    const iframe = await this.iframePromise_;
    sendGooglePhotosPhotosFunction(
        iframe.contentWindow!,
        googlePhotos?.slice(0, kMaximumGooglePhotosPreviews) ?? null);
  }

  /** Invoked on changes to the count of Google Photos photos. */
  private async onGooglePhotosCountChanged_(
      googlePhotosCount: number|null, googlePhotosCountLoading: boolean) {
    if (googlePhotosCountLoading || !isNullOrNumber(googlePhotosCount)) {
      return;
    }
    const iframe = await this.iframePromise_;
    sendGooglePhotosCountFunction(iframe.contentWindow!, googlePhotosCount);
  }

  /**
   * Send updated local images list to the iframe.
   */
  private async onLocalImagesChanged_(
      localImages: FilePath[]|null, localImagesLoading: boolean) {
    this.didSendLocalImageData_ = false;
    if (!localImagesLoading && Array.isArray(localImages)) {
      const iframe = await this.iframePromise_;
      sendLocalImagesFunction(iframe.contentWindow!, localImages);
    }
  }

  /**
   * Send up to |maximumImageThumbnailsCount| image thumbnails to untrusted.
   */
  private async onLocalImageDataChanged_(
      images: FilePath[]|null, imageData: Record<string, string>,
      imageDataLoading: Record<string, boolean>) {
    if (!Array.isArray(images) || !imageData || !imageDataLoading ||
        this.didSendLocalImageData_) {
      return;
    }

    const successfullyLoaded: string[] =
        images.map(image => image.path).filter(key => {
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
          images.every(image => imageDataLoading[image.path] === false);
    }


    if (shouldSendImageData()) {
      // Also send information about which images failed to load. This is
      // necessary to decide whether to show loading animation or failure svg
      // while updating local images.
      const failures = images.map(image => image.path)
                           .filter(key => {
                             const doneLoading =
                                 imageDataLoading[key] === false;
                             const failure = imageData[key] === '';
                             return failure && doneLoading;
                           })
                           .reduce((result, key) => {
                             // Empty string means that this image failed to
                             // load.
                             result[key] = '';
                             return result;
                           }, {} as Record<string, string>);

      const data =
          successfullyLoaded.filter((_, i) => i < kMaximumLocalImagePreviews)
              .reduce((result, key) => {
                result[key] = imageData[key];
                return result;
              }, failures);

      this.didSendLocalImageData_ = true;

      const iframe = await this.iframePromise_;
      sendLocalImageDataFunction(iframe.contentWindow!, data);
    }
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);
