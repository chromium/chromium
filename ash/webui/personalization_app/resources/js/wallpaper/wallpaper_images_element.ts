// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../../css/wallpaper.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {CurrentWallpaper, OnlineImageType, WallpaperCollection, WallpaperImage, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {isDarkLightModeEnabled} from '../load_time_booleans.js';
import {PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isNonEmptyArray} from '../utils.js';

import {ImageTile} from './constants.js';
import {getLoadingPlaceholderAnimationDelay, getLoadingPlaceholders, isWallpaperImage} from './utils.js';
import {selectWallpaper} from './wallpaper_controller.js';
import {WallpaperGridItemSelectedEvent} from './wallpaper_grid_item_element';
import {getTemplate} from './wallpaper_images_element.html.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

(BigInt.prototype as any).toJSON = function() {
  return this.toString();
};

/**
 * If |current| is set and is an online wallpaper (include daily refresh
 * wallpaper), return the assetId of that image. Otherwise returns null.
 */
function getAssetId(current: CurrentWallpaper|null): bigint|null {
  if (current == null) {
    return null;
  }
  if (current.type !== WallpaperType.kOnline &&
      current.type !== WallpaperType.kDaily) {
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

export class WallpaperImages extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-images';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether dark mode is the active preferred color scheme.
       */
      isDarkModeActive: {
        type: Boolean,
        value: false,
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

      selectedAssetId_: {
        type: BigInt,
        value: null,
      },

      /**
       * The pending selected image.
       */
      pendingSelectedAssetId_: {
        type: BigInt,
        value: null,
      },

      hasError_: {
        type: Boolean,
        computed:
            'computeHasError_(images_, imagesLoading_, collections_, collectionsLoading_, collectionId)',
        observer: 'onHasErrorChanged_',
      },


      tiles_: {
        type: Array,
        computed:
            'computeTiles_(images_, imagesLoading_, collectionId, isDarkModeActive)',
      },

    };
  }

  collectionId: string;
  isDarkModeActive: boolean;
  private collections_: WallpaperCollection[]|null;
  private collectionsLoading_: boolean;
  private images_: Record<string, WallpaperImage[]|null>;
  private imagesLoading_: Record<string, boolean>;
  private selectedAssetId_: bigint|null;
  private pendingSelectedAssetId_: bigint|null;
  private hasError_: boolean;
  private tiles_: ImageTile[];

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
    this.watch<WallpaperImages['selectedAssetId_']>(
        'selectedAssetId_',
        state => getAssetId(state.wallpaper.currentSelected));
    this.watch<WallpaperImages['pendingSelectedAssetId_']>(
        'pendingSelectedAssetId_',
        state => isWallpaperImage(state.wallpaper.pendingSelected) ?
            state.wallpaper.pendingSelected.assetId :
            null);
    this.updateFromStore();
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

  /** Kick the user back to wallpaper collections page if failed to load. */
  private onHasErrorChanged_(hasError: boolean) {
    if (hasError) {
      console.warn('An error occurred while loading collections or images');
      // Navigate back to main page and refresh.
      PersonalizationRouter.reloadAtWallpaper();
    }
  }

  /**
   * Send images if loading is ready and we have some images. Punt back to
   * main page if there is an error viewing this collection.
   */
  private computeTiles_(
      images: Record<string, WallpaperImage[]>,
      imagesLoading: Record<string, boolean>, collectionId: string,
      isDarkModeActive: boolean): ImageTile[]|number[] {
    const hasImages = !!images && !!imagesLoading && collectionId &&
        imagesLoading[collectionId] === false &&
        isNonEmptyArray(images[collectionId]);

    if (!hasImages) {
      return getLoadingPlaceholders(() => 1);
    }

    const imageArr = images[collectionId]!;

    if (isDarkLightModeEnabled()) {
      return getDarkLightImageTiles(isDarkModeActive, imageArr);
    } else {
      return getRegularImageTiles(imageArr);
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

  private isLoadingTile_(tile: number|ImageTile): tile is number {
    return typeof tile === 'number';
  }

  private isImageTile_(tile: number|ImageTile): tile is ImageTile {
    return tile.hasOwnProperty('preview') &&
        Array.isArray((tile as any).preview);
  }

  private getLoadingPlaceholderAnimationDelay_(index: number): string {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  private isTileSelected_(
      tile: ImageTile, selectedAssetId: bigint|null,
      pendingSelectedAssetId: bigint|null): boolean {
    // Make sure that both are bigint (not undefined) and equal.
    return (
        typeof selectedAssetId === 'bigint' && !!tile &&
            tile.assetId === selectedAssetId && !pendingSelectedAssetId ||
        typeof pendingSelectedAssetId === 'bigint' && !!tile &&
            tile.assetId === pendingSelectedAssetId);
  }

  private onImageSelected_(e: WallpaperGridItemSelectedEvent&
                           {model: {item: ImageTile}}) {
    const assetId = e.model.item.assetId;
    assert(assetId && typeof assetId === 'bigint', 'assetId not found');
    const images = this.images_[this.collectionId]!;
    assert(isNonEmptyArray(images));
    const selectedImage = images.find(choice => choice.assetId === assetId);
    assert(selectedImage, 'could not find selected image');
    selectWallpaper(selectedImage, getWallpaperProvider(), this.getStore());
  }

  private getAriaLabel_(tile: number|ImageTile): string {
    if (this.isLoadingTile_(tile)) {
      return this.i18n('ariaLabelLoading');
    }
    return tile.attribution!.join(' ');
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }
}

customElements.define(WallpaperImages.is, WallpaperImages);
