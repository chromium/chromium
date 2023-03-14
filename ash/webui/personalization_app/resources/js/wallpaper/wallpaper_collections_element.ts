// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../css/wallpaper.css.js';
import '../../common/icons.html.js';
import '../../css/common.css.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GooglePhotosEnablementState, OnlineImageType, WallpaperCollection, WallpaperImage} from '../../personalization_app.mojom-webui.js';
import {isDarkLightModeEnabled, isGooglePhotosIntegrationEnabled} from '../load_time_booleans.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getCountText, isImageDataUrl, isNonEmptyArray, isSelectionEvent} from '../utils.js';

import {DefaultImageSymbol, kDefaultImageSymbol, kMaximumLocalImagePreviews} from './constants.js';
import {getLoadingPlaceholderAnimationDelay, getLoadingPlaceholders, getPathOrSymbol} from './utils.js';
import {getTemplate} from './wallpaper_collections_element.html.js';
import {initializeBackdropData} from './wallpaper_controller.js';
import {WallpaperGridItemSelectedEvent} from './wallpaper_grid_item_element.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const kGooglePhotosCollectionId = 'google_photos_';
const kLocalCollectionId = 'local_';

enum TileType {
  IMAGE_GOOGLE_PHOTOS = 'image_google_photos',
  IMAGE_LOCAL = 'image_local',
  IMAGE_ONLINE = 'image_online',
  LOADING = 'loading',
}

interface LoadingTile {
  type: TileType.LOADING;
}

interface GooglePhotosTile {
  disabled: boolean;
  id: typeof kGooglePhotosCollectionId;
  name: string;
  type: TileType.IMAGE_GOOGLE_PHOTOS;
  preview: [Url];
}

interface LocalTile {
  count: string;
  disabled: boolean;
  id: typeof kLocalCollectionId;
  name: string;
  preview: Url[];
  type: TileType.IMAGE_LOCAL;
}

interface OnlineTile {
  count: string;
  disabled: boolean;
  id: string;
  info: string;
  name: string;
  preview: Url[];
  type: TileType.IMAGE_ONLINE;
}

type Tile = LoadingTile|GooglePhotosTile|LocalTile|OnlineTile;

/**
 * Before switching entirely to WallpaperGridItem, have to deal with a mix of
 * keyboard and mouse events and a custom WallpaperGridItemSelected event.
 */
type OnCollectionSelectedEvent =
    (MouseEvent|KeyboardEvent|WallpaperGridItemSelectedEvent)&
    {model: {item: Tile}};

function collectionsError(
    collections: WallpaperCollection[]|null,
    collectionsLoading: boolean): boolean {
  return !collectionsLoading && !isNonEmptyArray(collections);
}

function localImagesError(
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean): boolean {
  return !localImagesLoading && !isNonEmptyArray(localImages);
}

function hasError(
    collections: WallpaperCollection[]|null, collectionsLoading: boolean,
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean): boolean {
  return localImagesError(localImages, localImagesLoading) &&
      collectionsError(collections, collectionsLoading);
}

/** Returns the tile to display for the Google Photos collection. */
function getGooglePhotosTile(enablementState: GooglePhotosEnablementState):
    GooglePhotosTile {
  return {
    disabled: enablementState !== GooglePhotosEnablementState.kEnabled,
    id: kGooglePhotosCollectionId,
    name: loadTimeData.getString('googlePhotosLabel'),
    type: TileType.IMAGE_GOOGLE_PHOTOS,
    preview: [{url: 'chrome://personalization/images/google_photos.svg'}],
  };
}

function getImages(
    localImages: Array<FilePath|DefaultImageSymbol>,
    localImageData: Record<string|DefaultImageSymbol, Url>): Url[] {
  if (!localImageData || !Array.isArray(localImages)) {
    return [];
  }
  const result = [];
  for (const image of localImages) {
    const key = getPathOrSymbol(image);
    const data = localImageData[key];
    if (isImageDataUrl(data)) {
      result.push(data);
    }
    // Add at most |kMaximumLocalImagePreviews| thumbnail urls.
    if (result.length >= kMaximumLocalImagePreviews) {
      break;
    }
  }
  return result;
}

/**
 * A common display format between local images and WallpaperCollection.
 * Get the first displayable image with data from the list of possible images.
 */
function getLocalTile(
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean,
    localImageData: Record<FilePath['path']|DefaultImageSymbol, Url>):
    LocalTile|LoadingTile {
  if (localImagesLoading) {
    return {type: TileType.LOADING};
  }

  if (!localImages || localImages.length === 0) {
    return {
      count: getCountText(0),
      disabled: true,
      id: kLocalCollectionId,
      name: loadTimeData.getString('myImagesLabel'),
      preview: [{url: 'chrome://personalization/images/no_images.svg'}],
      type: TileType.IMAGE_LOCAL,
    };
  }

  const imagesToDisplay = getImages(localImages, localImageData);

  // Count all images that failed to load and subtract them from "My Images"
  // count.
  const failureCount = Object.values(localImageData).reduce((result, next) => {
    return !isImageDataUrl(next) ? result + 1 : result;
  }, 0);

  const successCount = localImages.length - failureCount;

  return {
    count: getCountText(successCount),
    disabled: successCount <= 0,
    id: kLocalCollectionId,
    name: loadTimeData.getString('myImagesLabel'),
    preview: imagesToDisplay,
    type: TileType.IMAGE_LOCAL,
  };
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

      images_: Object,

      imagesLoading_: Object,

      /**
       * Whether the user is allowed to access Google Photos.
       */
      googlePhotosEnabled_: {
        type: Number,
        observer: 'onGooglePhotosEnabledChanged_',
      },

      /**
       * Mapping of collection id to number of images. Loads in progressively
       * after collections_.
       */
      imageCounts_: {
        type: Object,
        computed: 'computeImageCounts_(images_, imagesLoading_)',
      },

      localImages_: Array,

      localImagesLoading_: Boolean,

      /**
       * Stores a mapping of local image id to thumbnail data.
       */
      localImageData_: {
        type: Object,
        value: {},
      },

      /**
       * List of tiles to be displayed to the user.
       */
      tiles_: {
        type: Array,
        value() {
          // Fill the view with loading tiles. Will be adjusted to the correct
          // number of tiles when collections are received.
          return getLoadingPlaceholders(
              (): LoadingTile => ({type: TileType.LOADING}));
        },
      },

      hasError_: Boolean,
    };
  }

  override hidden: boolean;
  private collections_: WallpaperCollection[]|null;
  private images_: Record<string, WallpaperImage[]|null>;
  private imagesLoading_: Record<string, boolean>;
  private imageCounts_: Record<string, number|null>;
  private googlePhotosEnabled_: GooglePhotosEnablementState|undefined;
  private localImages_: Array<FilePath|DefaultImageSymbol>|null;
  private localImagesLoading_: boolean;
  private localImageData_: Record<string|DefaultImageSymbol, Url>;
  private tiles_: Tile[];
  private hasError_: boolean;

  static get observers() {
    return [
      'onLocalImagesChanged_(localImages_, localImagesLoading_, localImageData_)',
      'onCollectionLoaded_(collections_, imageCounts_)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<WallpaperCollections['hasError_']>(
        'hasError_',
        state => hasError(
            state.wallpaper.backdrop.collections,
            state.wallpaper.loading.collections, state.wallpaper.local.images,
            state.wallpaper.loading.local.images));
    this.watch<WallpaperCollections['collections_']>(
        'collections_', state => state.wallpaper.backdrop.collections);
    this.watch<WallpaperCollections['images_']>(
        'images_', state => state.wallpaper.backdrop.images);
    this.watch<WallpaperCollections['imagesLoading_']>(
        'imagesLoading_', state => state.wallpaper.loading.images);
    this.watch<WallpaperCollections['googlePhotosEnabled_']>(
        'googlePhotosEnabled_', state => state.wallpaper.googlePhotos.enabled);
    this.watch<WallpaperCollections['localImages_']>(
        'localImages_', state => state.wallpaper.local.images);
    // Treat as loading if either loading local images list or loading the
    // default image thumbnail. This prevents rapid churning of the UI on first
    // load.
    this.watch<WallpaperCollections['localImagesLoading_']>(
        'localImagesLoading_',
        state => state.wallpaper.loading.local.images ||
            state.wallpaper.loading.local.data[kDefaultImageSymbol]);
    this.watch<WallpaperCollections['localImageData_']>(
        'localImageData_', state => state.wallpaper.local.data);
    this.updateFromStore();
    initializeBackdropData(getWallpaperProvider(), this.getStore());
  }

  /**
   * Notify that this element visibility has changed.
   */
  private async onHiddenChanged_(hidden: boolean) {
    if (!hidden) {
      document.title = this.i18n('wallpaperLabel');
    }
    afterNextRender(this, () => this.notifyResize());
  }

  /**
   * Calculate count of image units in each collection when a new collection is
   * fetched. D/L variants of the same image represent a count of 1.
   */
  private computeImageCounts_(
      images: Record<string, WallpaperImage[]|null>,
      imagesLoading: Record<string, boolean>): Record<string, number|null> {
    if (!images || !imagesLoading) {
      return {};
    }
    return Object.entries(images)
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
          result[key] = value;
          return result;
        }, {} as Record<string, number|null>);
  }

  private getLoadingPlaceholderAnimationDelay_(index: number): string {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  /**
   * Called each time a new collection finishes loading. |imageCounts| contains
   * a mapping of collection id to the number of images in that collection.
   * A value of null indicates that the given collection id has failed to load.
   */
  private onCollectionLoaded_(
      collections: WallpaperCollection[]|null,
      imageCounts: Record<string, number|null>) {
    if (!isNonEmptyArray(collections) || !imageCounts) {
      return;
    }

    // The first tile in the collections grid is reserved for local images. The
    // second tile is reserved for Google Photos, provided that the integration
    // is enabled. The tile index of other collections must be `offset` so as
    // not to occupy reserved space.
    const offset = isGooglePhotosIntegrationEnabled() ? 2 : 1;

    if (this.tiles_.length < collections.length + offset) {
      this.push(
          'tiles_',
          ...Array.from(
              {length: collections.length + offset - this.tiles_.length},
              (): LoadingTile => ({type: TileType.LOADING})));
    }
    if (this.tiles_.length > collections.length + offset) {
      this.splice('tiles_', collections.length + offset);
    }

    collections.forEach((collection, i) => {
      const index = i + offset;
      const tile = this.tiles_[index];
      assert(
          isNonEmptyArray(collection.previews),
          `preview images required for collection ${collection.id}`);

      if (imageCounts[collection.id] === undefined) {
        return;
      }
      const count = getCountText(imageCounts[collection.id] || 0);
      if (tile.type !== TileType.IMAGE_ONLINE || count !== tile.count) {
        // Return all the previews in D/L mode to display the split view.
        // Otherwise, only the first preview is needed.
        const preview = isDarkLightModeEnabled() ? collection.previews :
                                                   [collection.previews[0]];

        const newTile: OnlineTile = {
          count,
          // If `imageCounts[collection.id]` is null, this collection failed to
          // load and the user cannot select it.
          disabled: imageCounts[collection.id] === null,
          id: collection.id,
          info: collection.description,
          name: collection.name,
          preview,
          type: TileType.IMAGE_ONLINE,
        };
        this.set(`tiles_.${index}`, newTile);
      }
    });
  }

  /** Invoked on changes to |googlePhotosEnabled_|. */
  private onGooglePhotosEnabledChanged_(
      googlePhotosEnabled: WallpaperCollections['googlePhotosEnabled_']) {
    if (googlePhotosEnabled !== undefined) {
      const tile = getGooglePhotosTile(googlePhotosEnabled);
      this.set('tiles_.1', tile);
    }
  }

  /**
   * Called with updated local image list or local image thumbnail data when
   * either of those properties changes.
   */
  private onLocalImagesChanged_(
      localImages: Array<FilePath|DefaultImageSymbol>|null,
      localImagesLoading: boolean,
      localImageData: Record<FilePath['path']|DefaultImageSymbol, Url>) {
    const tile = getLocalTile(localImages, localImagesLoading, localImageData);
    this.set('tiles_.0', tile);
  }

  /** Navigate to the correct route based on user selection. */
  private onCollectionSelected_(e: OnCollectionSelectedEvent) {
    const tile = e.model.item;
    assert(!!tile, 'tile must be set to select');
    if (!this.isSelectableTile_(tile)) {
      // Ignore all events from disabled/loading tiles.
      return;
    }
    if (!(e instanceof WallpaperGridItemSelectedEvent) &&
        !isSelectionEvent(e)) {
      // While refactoring is in progress, may receive either a
      // `WallpaperGridItemSelectedEvent` or a mouse/keyboard selection event.
      // If not one of those two event types, ignore it and let it propagate.
      return;
    }
    switch (tile.id) {
      case kGooglePhotosCollectionId:
        PersonalizationRouter.instance().goToRoute(
            Paths.GOOGLE_PHOTOS_COLLECTION);
        return;
      case kLocalCollectionId:
        PersonalizationRouter.instance().goToRoute(Paths.LOCAL_COLLECTION);
        return;
      default:
        assert(
            isNonEmptyArray(this.collections_), 'collections array required');
        const collection =
            this.collections_.find(collection => collection.id === tile.id);
        assert(collection, 'collection with matching id required');
        PersonalizationRouter.instance().selectCollection(collection);
        return;
    }
  }

  private isLoadingTile_(item: Tile|null): item is LoadingTile {
    return !!item && item.type === TileType.LOADING;
  }

  private isLocalTile_(item: Tile|null): item is LocalTile {
    return !!item && item.type === TileType.IMAGE_LOCAL;
  }

  private isOnlineTile_(item: Tile|null): item is OnlineTile {
    return !!item && item.type === TileType.IMAGE_ONLINE;
  }

  private isGooglePhotosTile_(item: Tile|null): item is GooglePhotosTile {
    return !!item && item.type === TileType.IMAGE_GOOGLE_PHOTOS;
  }

  private isSelectableTile_(item: Tile|null): item is GooglePhotosTile|LocalTile
      |OnlineTile {
    return !!item && !this.isLoadingTile_(item) && !item.disabled;
  }

  private isTimeOfDayCollection_(item: Tile|null): boolean {
    return this.isOnlineTile_(item) &&
        (this.images_[item.id] || [])
            .some(
                ({type}) => type === OnlineImageType.kMorning ||
                    type === OnlineImageType.kLateAfternoon);
  }

  private getAriaIndex_(index: number): number {
    return index + 1;
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);
