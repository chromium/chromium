// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {WallpaperGridItemSelectedEvent} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';

import {CurrentWallpaper, OnlineImageType, WallpaperCollection, WallpaperImage, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {dismissTimeOfDayBanner} from '../ambient/ambient_controller.js';
import {isTimeOfDayWallpaperEnabled} from '../load_time_booleans.js';
import {PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {setColorModeAutoSchedule} from '../theme/theme_controller.js';
import {getThemeProvider} from '../theme/theme_interface_provider.js';
import {ThemeObserver} from '../theme/theme_observer.js';

import {ImageTile} from './constants.js';
import {getLoadingPlaceholderAnimationDelay, getLoadingPlaceholders, isWallpaperImage} from './utils.js';
import {getShouldShowTimeOfDayWallpaperDialog, selectWallpaper} from './wallpaper_controller.js';
import {getTemplate} from './wallpaper_images_element.html.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

(BigInt.prototype as any).toJSON = function() {
  return this.toString();
};

/**
 * If |current| is set and is an online wallpaper (include daily refresh
 * wallpaper), return the unitId of that image. Otherwise returns null.
 */
function getUnitId(current: CurrentWallpaper|null): bigint|null {
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
 * Return a list of tiles capturing units of image variants.
 */
export function getImageTiles(
    isDarkModeActive: boolean, images: WallpaperImage[]): ImageTile[] {
  const tileMap = images.reduce((result, next) => {
    if (result.has(next.unitId)) {
      const tile = result.get(next.unitId)! as ImageTile;
      if (!tile.hasPreviewImage) {
        tile.preview.push(next.url);
      }
    } else {
      result.set(next.unitId, {
        preview: [next.url],
        unitId: next.unitId,
      } as ImageTile);
    }
    // Populate the assetId and attribution based on image type and system's
    // color mode.
    const tile = result.get(next.unitId)! as ImageTile;
    switch (next.type) {
      case OnlineImageType.kLight:
        if (!isDarkModeActive) {
          tile.assetId = next.assetId;
          tile.attribution = next.attribution;
        }
        break;
      case OnlineImageType.kDark:
        if (isDarkModeActive) {
          tile.assetId = next.assetId;
          tile.attribution = next.attribution;
        }
        break;
      case OnlineImageType.kMorning:
      case OnlineImageType.kLateAfternoon:
        tile.isTimeOfDayWallpaper = true;
        tile.assetId = next.assetId;
        tile.attribution = next.attribution;
        break;
      case OnlineImageType.kPreview:
        tile.hasPreviewImage = true;
        tile.preview = [next.url];
        tile.assetId = next.assetId;
        tile.attribution = next.attribution;
        break;
      case OnlineImageType.kUnknown:
        tile.assetId = next.assetId;
        tile.attribution = next.attribution;
        break;
    }
    return result;
  }, new Map() as Map<bigint, ImageTile>);
  return [...tileMap.values()];
}

export class WallpaperImagesElement extends WithPersonalizationStore {
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

      selectedUnitId_: {
        type: BigInt,
        value: null,
      },

      /**
       * The pending selected image.
       */
      pendingSelectedUnitId_: {
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
        observer: 'onTilesChanged_',
      },

      /**
       * The pending ToD wallpaper to be set when the dialog is displayed.
       */
      pendingTimeOfDayWallpaper_: Object,

      colorModeAutoScheduleEnabled_: Boolean,

      showTimeOfDayWallpaperDialog_: Boolean,
    };
  }

  collectionId: string;
  isDarkModeActive: boolean;
  private collections_: WallpaperCollection[]|null;
  private collectionsLoading_: boolean;
  private images_: Record<string, WallpaperImage[]|null>;
  private imagesLoading_: Record<string, boolean>;
  private selectedUnitId_: bigint|null;
  private pendingSelectedUnitId_: bigint|null;
  private hasError_: boolean;
  private tiles_: ImageTile[];
  private pendingTimeOfDayWallpaper_: WallpaperImage|null;
  private colorModeAutoScheduleEnabled_: boolean|null;
  private showTimeOfDayWallpaperDialog_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    ThemeObserver.initThemeObserverIfNeeded();
    this.watch<WallpaperImagesElement['images_']>(
        'images_', state => state.wallpaper.backdrop.images);
    this.watch<WallpaperImagesElement['imagesLoading_']>(
        'imagesLoading_', state => state.wallpaper.loading.images);
    this.watch<WallpaperImagesElement['collections_']>(
        'collections_', state => state.wallpaper.backdrop.collections);
    this.watch<WallpaperImagesElement['collectionsLoading_']>(
        'collectionsLoading_', state => state.wallpaper.loading.collections);
    this.watch<WallpaperImagesElement['selectedUnitId_']>(
        'selectedUnitId_', state => getUnitId(state.wallpaper.currentSelected));
    this.watch<WallpaperImagesElement['pendingSelectedUnitId_']>(
        'pendingSelectedUnitId_',
        state => isWallpaperImage(state.wallpaper.pendingSelected) ?
            state.wallpaper.pendingSelected.unitId :
            null);
    this.watch<WallpaperImagesElement['colorModeAutoScheduleEnabled_']>(
        'colorModeAutoScheduleEnabled_',
        state => state.theme.colorModeAutoScheduleEnabled);
    this.watch<WallpaperImagesElement['showTimeOfDayWallpaperDialog_']>(
        'showTimeOfDayWallpaperDialog_',
        state => state.wallpaper.shouldShowTimeOfDayWallpaperDialog);
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
      PersonalizationRouterElement.reloadAtWallpaper();
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
    return getImageTiles(isDarkModeActive, imageArr);
  }

  private onTilesChanged_(tiles: ImageTile[]) {
    if (tiles.some((tile => this.isTimeOfDayWallpaper_(tile)))) {
      // Dismisses the banner after the Time of Day collection images are
      // displayed.
      dismissTimeOfDayBanner(this.getStore());
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
      tile: ImageTile, selectedUnitId: bigint|null,
      pendingSelectedUnitId: bigint|null): boolean {
    // Make sure that both are bigint (not undefined) and equal.
    return (
        typeof selectedUnitId === 'bigint' && !!tile &&
            tile.unitId === selectedUnitId && !pendingSelectedUnitId ||
        typeof pendingSelectedUnitId === 'bigint' && !!tile &&
            tile.unitId === pendingSelectedUnitId);
  }

  private isTimeOfDayWallpaper_(tile: number|ImageTile): boolean {
    return this.isImageTile_(tile) && !!tile.isTimeOfDayWallpaper;
  }

  private async onImageSelected_(e: WallpaperGridItemSelectedEvent&
                                 {model: {item: ImageTile}}) {
    const unitId = e.model.item.unitId;
    assert(unitId && typeof unitId === 'bigint', 'unitId not found');
    const images = this.images_[this.collectionId]!;
    assert(isNonEmptyArray(images));
    const selectedImage = images.find(choice => choice.unitId === unitId);
    assert(selectedImage, 'could not find selected image');
    if (await this.shouldShowTimeOfDayWallpaperDialog_(e.model.item)) {
      this.pendingTimeOfDayWallpaper_ = selectedImage;
      return;
    }
    selectWallpaper(selectedImage, getWallpaperProvider(), this.getStore());
  }

  private async shouldShowTimeOfDayWallpaperDialog_(tile: ImageTile):
      Promise<boolean> {
    if (isTimeOfDayWallpaperEnabled()) {
      await getShouldShowTimeOfDayWallpaperDialog(
          getWallpaperProvider(), this.getStore());
    }
    return this.isTimeOfDayWallpaper_(tile) &&
        this.showTimeOfDayWallpaperDialog_ &&
        !this.colorModeAutoScheduleEnabled_;
  }

  private onCloseTimeOfDayDialog_() {
    assert(
        this.pendingTimeOfDayWallpaper_,
        'could not find the time of day wallpaper');
    selectWallpaper(
        this.pendingTimeOfDayWallpaper_, getWallpaperProvider(),
        this.getStore());
    this.pendingTimeOfDayWallpaper_ = null;
  }

  private onConfirmTimeOfDayDialog_() {
    setColorModeAutoSchedule(
        /*enabled=*/ true, getThemeProvider(), this.getStore());
    this.onCloseTimeOfDayDialog_();
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

customElements.define(WallpaperImagesElement.is, WallpaperImagesElement);
