// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects.
 */

import './styles.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {kMaximumLocalImagePreviews} from '../common/constants.js';
import {sendCollections, sendImageCounts, sendLocalImageData, sendLocalImages, sendVisible} from '../common/iframe_api.js';
import {isNonEmptyArray, promisifyOnload} from '../common/utils.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {initializeBackdropData} from './personalization_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';

let sendCollectionsFunction = sendCollections;
let sendImageCountsFunction = sendImageCounts;
let sendLocalImagesFunction = sendLocalImages;
let sendLocalImageDataFunction = sendLocalImageData;

/**
 * Mock out the iframe api functions for testing. Return promises that are
 * resolved when the function is called by |WallpaperCollectionsElement|.
 * @return {{
 *   sendCollections: Promise<?>,
 *   sendImageCounts: Promise<?>,
 *   sendLocalImages: Promise<?>,
 *   sendLocalImageData: Promise<?>,
 * }}
 */
export function promisifyIframeFunctionsForTesting() {
  let resolvers = {};
  const promises = [
    sendCollections, sendImageCounts, sendLocalImages, sendLocalImageData
  ].reduce((result, next) => {
    result[next.name] = new Promise(resolve => resolvers[next.name] = resolve);
    return result;
  }, {});
  sendCollectionsFunction = (...args) => resolvers[sendCollections.name](args);
  sendImageCountsFunction = (...args) => resolvers[sendImageCounts.name](args);
  sendLocalImagesFunction = (...args) => resolvers[sendLocalImages.name](args);
  sendLocalImageDataFunction = (...args) =>
      resolvers[sendLocalImageData.name](args);
  return promises;
}

/** @polymer */
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

      /**
       * @type {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
       * @private
       */
      collections_: {
        type: Array,
        observer: 'onCollectionsChanged_',
      },

      /** @private */
      collectionsLoading_: {
        type: Boolean,
      },

      /**
       * Contains a mapping of collection id to an array of images.
       * @type {Object<string,
       *     Array<!chromeos.personalizationApp.mojom.WallpaperImage>>}
       * @private
       */
      images_: {
        type: Object,
      },

      /**
       * Contains a mapping of collection id to loading boolean.
       * @type {Object<string, boolean>}
       * @private
       */
      imagesLoading_: {
        type: Object,
      },

      /**
       * @type {Array<!mojoBase.mojom.FilePath>}
       * @private
       */
      localImages_: {
        type: Array,
        observer: 'onLocalImagesChanged_',
      },

      /**
       * Stores a mapping of local image id to loading status.
       * @type {!Object<string, boolean>}
       * @private
       */
      localImageDataLoading_: {
        type: Object,
      },

      /**
       * Stores a mapping of local image id to thumbnail data.
       * @type {Object<string, string>}
       * @private
       */
      localImageData_: {
        type: Object,
      },

      /** @private */
      hasError_: {
        type: Boolean,
        // Call computed functions with their dependencies as arguments so that
        // polymer knows when to re-run the computation.
        computed: 'computeHasError_(collections_, collectionsLoading_)',
      },
    };
  }

  static get observers() {
    return [
      'onCollectionImagesChanged_(images_, imagesLoading_)',
      'onLocalImageDataChanged_(localImages_, localImageData_, localImageDataLoading_)',
    ];
  }

  /** @override */
  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
    this.iframePromise_ = /** @type {!Promise<!HTMLIFrameElement>} */ (
        promisifyOnload(this, 'collections-iframe', afterNextRender));

    /**
     * @type {boolean}
     */
    this.didSendLocalImageData_ = false;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.backdrop.collections);
    this.watch('collectionsLoading_', state => state.loading.collections);
    this.watch('images_', state => state.backdrop.images);
    this.watch('imagesLoading_', state => state.loading.images);
    this.watch('localImages_', state => state.local.images);
    this.watch('localImageData_', state => state.local.data);
    this.watch('localImageDataLoading_', state => state.loading.local.data);
    this.updateFromStore();
    initializeBackdropData(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Notify iframe that this element visibility has changed.
   * @param {boolean} hidden
   * @private
   */
  async onHiddenChanged_(hidden) {
    if (!hidden) {
      document.title = this.i18n('title');
    }
    const iframe = await this.iframePromise_;
    sendVisible(/** @type {!Window} */ (iframe.contentWindow), !hidden);
  }

  /**
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @param {boolean} loading
   * @return {boolean}
   * @private
   */
  computeHasError_(collections, loading) {
    return !loading && !isNonEmptyArray(collections);
  }

  /**
   * Send updated wallpaper collections to the iframe.
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @private
   */
  async onCollectionsChanged_(collections) {
    if (isNonEmptyArray(collections)) {
      const iframe = await this.iframePromise_;
      sendCollectionsFunction(iframe.contentWindow, collections);
    }
  }

  /**
   * Send count of images in each collection when a new collection is fetched.
   * @param {Object<string,
   *     Array<!chromeos.personalizationApp.mojom.WallpaperImage>>} images
   * @param {Object<string, boolean>} imagesLoading
   * @private
   */
  async onCollectionImagesChanged_(images, imagesLoading) {
    if (!images || !imagesLoading) {
      return;
    }
    const counts =
        Object.entries(images)
            .filter(([collectionId]) => {
              return imagesLoading[/** @type {string} */ (collectionId)] ===
                  false;
            })
            .map(([key, value]) => {
              // Collection has completed loading. If no images were retrieved,
              // set count value to null to indicate failure.
              return [key, Array.isArray(value) ? value.length : null];
            })
            .reduce((result, [key, value]) => {
              result[key] = value;
              return result;
            }, {});
    const iframe = await this.iframePromise_;
    sendImageCountsFunction(
        /** @type {!Window} */ (iframe.contentWindow), counts);
  }

  /**
   * Send updated local images list to the iframe.
   * @param {?Array<!mojoBase.mojom.FilePath>} value
   * @private
   */
  async onLocalImagesChanged_(value) {
    this.didSendLocalImageData_ = false;
    if (Array.isArray(value)) {
      const iframe = await this.iframePromise_;
      sendLocalImagesFunction(
          /** @type {!Window} */ (iframe.contentWindow), value);
    }
  }

  /**
   * Send up to |maximumImageThumbnailsCount| image thumbnails to untrusted.
   * @param {?Array<!mojoBase.mojom.FilePath>} images
   * @param {?Object<string, string>} imageData
   * @param {?Object<string, boolean>} imageDataLoading
   * @private
   */
  async onLocalImageDataChanged_(images, imageData, imageDataLoading) {
    if (!Array.isArray(images) || !imageData || !imageDataLoading ||
        this.didSendLocalImageData_) {
      return;
    }

    /** @type !Array<string> */
    const successfullyLoaded = images.map(image => image.path).filter(key => {
      const doneLoading = imageDataLoading[key] === false;
      const success = !!imageData[key];
      return success && doneLoading;
    });

    /**
     * @return {boolean}
     */
    function shouldSendImageData() {
      // All images (up to |kMaximumLocalImagePreviews|) have loaded.
      const didLoadMaximum = successfullyLoaded.length >=
          Math.min(kMaximumLocalImagePreviews, images.length);

      return didLoadMaximum ||
          // No more images to load so send now even if some failed.
          images.every(image => imageDataLoading[image.path] === false);
    };


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
                           }, {});

      const data =
          successfullyLoaded.filter((_, i) => i < kMaximumLocalImagePreviews)
              .reduce((result, key) => {
                result[key] = imageData[key];
                return result;
              }, failures);

      this.didSendLocalImageData_ = true;

      const iframe = await this.iframePromise_;
      sendLocalImageDataFunction(
          /** @type {!Window} */ (iframe.contentWindow), data);
    }
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);
