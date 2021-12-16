// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './styles.js';

import {ImageTile} from '/common/constants.js';
import {isNonEmptyArray, promisifyOnload} from '/common/utils.js';
import {sendCurrentWallpaperAssetId, sendImageTiles, sendPendingWallpaperAssetId, sendVisible} from '/trusted/iframe_api.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DisplayableImage, OnlineImageType, WallpaperType} from '../personalization_app.mojom-webui.js';
import {PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

let sendCurrentWallpaperAssetIdFunction = sendCurrentWallpaperAssetId;
let sendImageTilesFunction = sendImageTiles;

/**
 * Mock out the images iframe api functions for testing. Return promises that
 * are resolved when the function is called by |WallpaperImagesElement|.
 * @return {{
 *   sendCurrentWallpaperAssetId: Promise<?>,
 *   sendImageTiles: Promise<?>,
 * }}
 */
export function promisifyImagesIframeFunctionsForTesting() {
  const resolvers = {};
  const promises =
      [sendCurrentWallpaperAssetId, sendImageTiles].reduce((result, next) => {
        result[next.name] =
            new Promise(resolve => resolvers[next.name] = resolve);
        return result;
      }, {});
  sendCurrentWallpaperAssetIdFunction = (...args) =>
      resolvers[sendCurrentWallpaperAssetId.name](args);
  sendImageTilesFunction = (...args) => resolvers[sendImageTiles.name](args);
  return promises;
}

/**
 * If |current| is set and is an online wallpaper (include daily refresh
 * wallpaper), return the assetId of that image. Otherwise returns null.
 * @param {?CurrentWallpaper} current
 * @return {?bigint}
 */
function getAssetId(current) {
  const currentType = current?.type;
  if (!(currentType === WallpaperType.kOnline ||
        currentType === WallpaperType.kDaily)) {
    return null;
  }
  try {
    return BigInt(current.key);
  } catch (e) {
    console.warn('Required a BigInt value here', e);
    return null;
  }
}

/**
 * Get the loading status of this page.
 * If collections are still loading, or if the specific collection with id
 * |collectionId| is still loading, the page is considered to be loading.
 * @param {?boolean} collectionsLoading
 * @param {?Object<string, boolean>} imagesLoading
 * @param {?string} collectionId
 * @return {boolean}
 * @private
 */
function isLoading(collectionsLoading, imagesLoading, collectionId) {
  if (!imagesLoading || !collectionId) {
    return true;
  }
  return collectionsLoading || (imagesLoading[collectionId] !== false);
}

/**
 * Return a list of tile where each tile contains a single image.
 * @param {!Array<!ash.personalizationApp.mojom.WallpaperImage>} images
 * @return {!Array<!ImageTile>}
 */
export function getRegularImageTiles(images) {
  return images.reduce((result, next) => {
    result.push({
      assetId: next.assetId,
      attribution: next.attribution,
      preview: [next.url],
    });
    return result;
  }, []);
}

/**
 * Return a list of tiles capturing units of Dark/Light images.
 * @param {!Array<!ash.personalizationApp.mojom.WallpaperImage>} images
 * @param {boolean} isDarkModeActive
 * @return {!Array<!ImageTile>}
 */
export function getDarkLightImageTiles(isDarkModeActive, images) {
  const tileMap = images.reduce((result, next) => {
    if (next.unitId in result) {
      // Add light url to the front and dark url to the back of the preview.
      if (next.type === OnlineImageType.kLight) {
        result[next.unitId]['preview'].unshift(next.url);
      } else {
        result[next.unitId]['preview'].push(next.url);
      }
    } else {
      result[next.unitId] = {
        preview: [next.url],
        unitId: next.unitId,
      };
    }
    // Populate the assetId and attribution based on image type and system's
    // color mode.
    if ((isDarkModeActive && next.type !== OnlineImageType.kLight) ||
        (!isDarkModeActive && next.type !== OnlineImageType.kDark)) {
      result[next.unitId]['assetId'] = next.assetId;
      result[next.unitId]['attribution'] = next.attribution;
    }
    return result;
  }, {});
  return Object.values(tileMap);
}

/** @polymer */
export class WallpaperImages extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-images';
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
       * The current collection id to display.
       */
      collectionId: {
        type: String,
      },

      /**
       * @type {?Array<!WallpaperCollection>}
       */
      collections_: {
        type: Array,
      },

      /** @private */
      collectionsLoading_: {
        type: Boolean,
      },

      /**
       * @type {!Object<string,
       *     ?Array<!WallpaperImage>>}
       * @private
       */
      images_: {
        type: Object,
      },

      /**
       * Mapping of collection_id to boolean.
       * @type {!Object<string, boolean>}
       */
      imagesLoading_: {
        type: Object,
      },

      /**
       * @type {?CurrentWallpaper}
       */
      currentSelected_: {
        type: Object,
        observer: 'onCurrentSelectedChanged_',
      },

      /**
       * The pending selected image.
       * @type {?DisplayableImage}
       * @private
       */
      pendingSelected_: {
        type: Object,
        observer: 'onPendingSelectedChanged_',
      },

      /** @private */
      hasError_: {
        type: Boolean,
        // Call computed functions with their dependencies as arguments so that
        // polymer knows when to re-run the computation.
        computed:
            'computeHasError_(images_, imagesLoading_, collections_, collectionsLoading_, collectionId)',
      },

      /**
       * In order to prevent re-sending images every time a collection loads in
       * the background, calculate this intermediate boolean. That way
       * |onImagesUpdated_| will re-run whenever this value flips from false to
       * true, rather than each time a new collection is changed in the
       * background.
       * @private
       */
      hasImages_: {
        type: Boolean,
        computed: 'computeHasImages_(images_, imagesLoading_, collectionId)',
      },

      /**
       * Whether dark mode is the active preferred color scheme.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'onImagesUpdated_(hasImages_, hasError_, collectionId, isDarkModeActive_)',
    ];
  }

  /** @override */
  constructor() {
    super();
    this.iframePromise_ = /** @type {!Promise<!HTMLIFrameElement>} */ (
        promisifyOnload(this, 'images-iframe', afterNextRender));
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('images_', state => state.wallpaper.backdrop.images);
    this.watch('imagesLoading_', state => state.wallpaper.loading.images);
    this.watch('collections_', state => state.wallpaper.backdrop.collections);
    this.watch(
        'collectionsLoading_', state => state.wallpaper.loading.collections);
    this.watch('currentSelected_', state => state.wallpaper.currentSelected);
    this.watch('pendingSelected_', state => state.wallpaper.pendingSelected);
    this.updateFromStore();
  }

  /**
   * Notify iframe that this element visibility has changed.
   * @param {boolean} hidden
   * @private
   */
  async onHiddenChanged_(hidden) {
    if (!hidden) {
      this.shadowRoot.getElementById('main').focus();
    }
    const iframe = await this.iframePromise_;
    sendVisible(/** @type {!Window} */ (iframe.contentWindow), !hidden);
  }

  /**
   * @param {?CurrentWallpaper} selected
   * @private
   */
  async onCurrentSelectedChanged_(selected) {
    const assetId = getAssetId(selected);
    const iframe = await this.iframePromise_;
    sendCurrentWallpaperAssetIdFunction(
        /** @type {!Window} */ (iframe.contentWindow), assetId);
  }

  /**
   * @param {?DisplayableImage} pendingSelected
   * @private
   */
  async onPendingSelectedChanged_(pendingSelected) {
    const iframe = await this.iframePromise_;
    sendPendingWallpaperAssetId(
        /** @type {!Window} */ (iframe.contentWindow),
        pendingSelected?.assetId || null);
  }

  /**
   * Determine whether the current collection failed to load or is not a valid
   * |collectionId|. Check that collections list loaded successfully, and that
   * the collection with id |collectionId| also loaded successfully.
   * @param {?Object<string,
   *     Array<!WallpaperImage>>} images
   * @param {?Object<string, boolean>} imagesLoading
   * @param {?Array<!WallpaperCollection>}
   *     collections
   * @param {boolean} collectionsLoading
   * @param {string} collectionId
   * @return {boolean}
   * @private
   */
  computeHasError_(
      images, imagesLoading, collections, collectionsLoading, collectionId) {
    // Not yet initialized or still loading.
    if (!imagesLoading || !collectionId || collectionsLoading) {
      return false;
    }

    // Failed to load collections or unknown collectionId.
    if (!isNonEmptyArray(collections) ||
        !collections.some(collection => collection.id === collectionId)) {
      return true;
    }

    // Specifically check === false to guarantee that key is in the object and
    // set as false.
    return imagesLoading[collectionId] === false &&
        !isNonEmptyArray(images[collectionId]);
  }

  /**
   * @param {?Object<string,
   *     Array<!WallpaperImage>>} images
   * @param {Object<string, boolean>} imagesLoading
   * @param {string} collectionId
   * @return {boolean}
   * @private
   */
  computeHasImages_(images, imagesLoading, collectionId) {
    return !!images && !!imagesLoading &&
        // Specifically check === false again here.
        imagesLoading[collectionId] === false &&
        isNonEmptyArray(images[collectionId]);
  }

  /**
   * Send images if loading is ready and we have some images. Punt back to
   * main page if there is an error viewing this collection.
   * @param {boolean} hasImages
   * @param {boolean} hasError
   * @param {string} collectionId
   * @param {boolean} isDarkModeActive
   * @private
   */
  async onImagesUpdated_(hasImages, hasError, collectionId, isDarkModeActive) {
    if (hasError) {
      console.warn('An error occurred while loading collections or images');
      // Navigate back to main page and refresh.
      PersonalizationRouter.reloadAtWallpaper();
      return;
    }

    if (hasImages && collectionId) {
      const iframe = await this.iframePromise_;
      const imageArr =
          /** @type {!Array<!ash.personalizationApp.mojom.WallpaperImage>} */ (
              this.images_[collectionId]);
      const isDarkLightModeEnabled =
          loadTimeData.getBoolean('isDarkLightModeEnabled');
      if (isDarkLightModeEnabled) {
        sendImageTilesFunction(
            iframe.contentWindow,
            getDarkLightImageTiles(isDarkModeActive, imageArr));
      } else {
        sendImageTilesFunction(
            iframe.contentWindow, getRegularImageTiles(imageArr));
      }
    }
  }


  /**
   * @private
   * @param {string} collectionId
   * @param {?Array<!WallpaperCollection>}
   *     collections
   * @return {string}
   */
  getMainAriaLabel_(collectionId, collections) {
    if (!collectionId || !Array.isArray(collections)) {
      return '';
    }
    const collection =
        collections.find(collection => collection.id === collectionId);

    if (!collection) {
      console.warn('Did not find collection matching collectionId');
      return '';
    }

    return collection.name;
  }
}

customElements.define(WallpaperImages.is, WallpaperImages);
