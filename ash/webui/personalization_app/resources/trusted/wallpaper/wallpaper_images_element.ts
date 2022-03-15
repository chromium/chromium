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

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ImageTile} from '../../common/constants.js';
import {isNonEmptyArray} from '../../common/utils.js';
import {ImagesGrid} from '../../untrusted/images_grid.js';
import {IFrameApi} from '../iframe_api.js';
import {CurrentWallpaper, OnlineImageType, WallpaperCollection, WallpaperImage, WallpaperType} from '../personalization_app.mojom-webui.js';
import {DisplayableImage} from '../personalization_reducers.js';
import {PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isWallpaperImage} from '../utils.js';

/**
 * If |current| is set and is an online wallpaper (include daily refresh
 * wallpaper), return the assetId of that image. Otherwise returns null.
 */
function getAssetId(current: CurrentWallpaper|null): bigint|undefined {
  if (current == null) {
    return undefined;
  }
  if (current.type !== WallpaperType.kOnline &&
      current.type !== WallpaperType.kDaily) {
    return undefined;
  }
  try {
    return BigInt(current.key);
  } catch (e) {
    console.warn('Required a BigInt value here', e);
    return undefined;
  }
}

/**
 * Return a list of tile where each tile contains a single image.
 */
export function getRegularImageTiles(images: WallpaperImage[]): ImageTile[] {
  return images.reduce((result, next) => {
    result.push({
      assetId: next.assetId,
      attribution: next.attribution,
      preview: [next.url],
    });
    return result;
  }, [] as ImageTile[]);
}

/**
 * Return a list of tiles capturing units of Dark/Light images.
 */
export function getDarkLightImageTiles(
    isDarkModeActive: boolean, images: WallpaperImage[]): ImageTile[] {
  const tileMap = images.reduce((result, next) => {
    if (result.has(next.unitId)) {
      // Add light url to the front and dark url to the back of the preview.
      if (next.type === OnlineImageType.kLight) {
        result.get(next.unitId)!['preview'].unshift(next.url);
      } else {
        result.get(next.unitId)!['preview'].push(next.url);
      }
    } else {
      result.set(next.unitId, {
        preview: [next.url],
        unitId: next.unitId,
      });
    }
    // Populate the assetId and attribution based on image type and system's
    // color mode.
    if ((isDarkModeActive && next.type !== OnlineImageType.kLight) ||
        (!isDarkModeActive && next.type !== OnlineImageType.kDark)) {
      result.get(next.unitId)!['assetId'] = next.assetId;
      result.get(next.unitId)!['attribution'] = next.attribution;
    }
    return result;
  }, new Map() as Map<bigint, ImageTile>);
  return [...tileMap.values()];
}

export interface WallpaperImages {
  $: {imagesGrid: ImagesGrid;}
}

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
      collectionId: String,

      collections_: Array,

      collectionsLoading_: Boolean,

      images_: Object,

      /**
       * Mapping of collection_id to boolean.
       */
      imagesLoading_: Object,

      currentSelected_: {
        type: Object,
        observer: 'onCurrentSelectedChanged_',
      },

      /**
       * The pending selected image.
       */
      pendingSelected_: {
        type: Object,
        observer: 'onPendingSelectedChanged_',
      },

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
       */
      hasImages_: {
        type: Boolean,
        computed: 'computeHasImages_(images_, imagesLoading_, collectionId)',
      },

      /**
       * Whether dark mode is the active preferred color scheme.
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  override hidden: boolean;
  collectionId: string;
  private collections_: WallpaperCollection[]|null;
  private collectionsLoading_: boolean;
  private images_: Record<string, WallpaperImage[]|null>;
  private imagesLoading_: Record<string, boolean>;
  private currentSelected_: CurrentWallpaper|null;
  private pendingSelected_: DisplayableImage|null;
  private hasError_: boolean;
  private hasImages_: boolean;
  private isDarkModeActive_: boolean;

  static get observers() {
    return [
      'onImagesUpdated_(hasImages_, hasError_, collectionId, isDarkModeActive_)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<WallpaperImages['images_']>(
        'images_', state => state.wallpaper.backdrop.images);
    this.watch<WallpaperImages['imagesLoading_']>(
        'imagesLoading_', state => state.wallpaper.loading.images);
    this.watch<WallpaperImages['collections_']>(
        'collections_', state => state.wallpaper.backdrop.collections);
    this.watch<WallpaperImages['collectionsLoading_']>(
        'collectionsLoading_', state => state.wallpaper.loading.collections);
    this.watch<WallpaperImages['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<WallpaperImages['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.updateFromStore();
  }

  /**
   * Notify iframe that this element visibility has changed.
   */
  private onHiddenChanged_(hidden: boolean) {
    if (!hidden) {
      this.shadowRoot!.getElementById('main')!.focus();
    }
    IFrameApi.getInstance().sendVisible(this.$.imagesGrid, !hidden);
  }

  private onCurrentSelectedChanged_(selected: CurrentWallpaper|null) {
    const assetId = getAssetId(selected);
    IFrameApi.getInstance().sendCurrentWallpaperAssetId(
        this.$.imagesGrid, assetId);
  }

  private onPendingSelectedChanged_(pendingSelected: DisplayableImage|null) {
    IFrameApi.getInstance().sendPendingWallpaperAssetId(
        this.$.imagesGrid,
        isWallpaperImage(pendingSelected) ? pendingSelected.assetId :
                                            undefined);
  }

  /**
   * Determine whether the current collection failed to load or is not a valid
   * |collectionId|. Check that collections list loaded successfully, and that
   * the collection with id |collectionId| also loaded successfully.
   */
  private computeHasError_(
      images: Record<string, WallpaperImage>,
      imagesLoading: Record<string, boolean>,
      collections: WallpaperCollection[], collectionsLoading: boolean,
      collectionId: string): boolean {
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

  private computeHasImages_(
      images: Record<string, WallpaperImage>,
      imagesLoading: Record<string, boolean>, collectionId: string): boolean {
    return !!images && !!imagesLoading &&
        // Specifically check === false again here.
        imagesLoading[collectionId] === false &&
        isNonEmptyArray(images[collectionId]);
  }

  /**
   * Send images if loading is ready and we have some images. Punt back to
   * main page if there is an error viewing this collection.
   */
  private onImagesUpdated_(
      hasImages: boolean, hasError: boolean, collectionId: string,
      isDarkModeActive: boolean) {
    if (hasError) {
      console.warn('An error occurred while loading collections or images');
      // Navigate back to main page and refresh.
      PersonalizationRouter.reloadAtWallpaper();
      return;
    }

    if (hasImages && collectionId) {
      const imageArr = this.images_[collectionId];
      const isDarkLightModeEnabled =
          loadTimeData.getBoolean('isDarkLightModeEnabled');
      if (isDarkLightModeEnabled) {
        IFrameApi.getInstance().sendImageTiles(
            this.$.imagesGrid,
            getDarkLightImageTiles(isDarkModeActive, imageArr!));
      } else {
        IFrameApi.getInstance().sendImageTiles(
            this.$.imagesGrid, getRegularImageTiles(imageArr!));
      }
    }
  }


  private getMainAriaLabel_(
      collectionId: string, collections: WallpaperCollection[]) {
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
