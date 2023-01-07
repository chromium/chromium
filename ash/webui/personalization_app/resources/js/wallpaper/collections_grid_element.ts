// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../common/icons.html.js';
import '../../css/common.css.js';
import '../../css/wallpaper.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {GooglePhotosEnablementState, WallpaperCollection, WallpaperImage} from '../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getCountText, isNonEmptyArray, isPngDataUrl, isSelectionEvent} from '../utils.js';

import {getTemplate} from './collections_grid_element.html.js';
import {DefaultImageSymbol, kDefaultImageSymbol, kMaximumLocalImagePreviews} from './constants.js';
import {getLoadingPlaceholderAnimationDelay, getLoadingPlaceholders, getPathOrSymbol} from './utils.js';

/**
 * @fileoverview Responds to |SendCollectionsEvent| from trusted. Handles user
 * input and responds with |SelectCollectionEvent| when an image is selected.
 */

const kGooglePhotosCollectionId = 'google_photos_';
const kLocalCollectionId = 'local_';

enum TileType {
  LOADING = 'loading',
  IMAGE_GOOGLE_PHOTOS = 'image_google_photos',
  IMAGE_LOCAL = 'image_local',
  IMAGE_ONLINE = 'image_online',
  FAILURE = 'failure',
}

interface LoadingTile {
  type: TileType.LOADING;
}

/**
 * Type that represents a collection that failed to load. The preview image
 * is still displayed, but is grayed out and unclickable.
 */
interface FailureTile {
  type: TileType.FAILURE;
  id: string;
  name: string;
  preview: [];
}

/**
 * A displayable type constructed from up to three LocalImages or a
 * WallpaperCollection.
 */
interface ImageTile {
  type: TileType.IMAGE_GOOGLE_PHOTOS|TileType.IMAGE_LOCAL|TileType.IMAGE_ONLINE;
  id: string;
  name: string;
  count?: string;
  preview: Url[];
}

type Tile = LoadingTile|FailureTile|ImageTile;

interface RepeaterEvent extends CustomEvent {
  model: {
    item: Tile,
  };
}

/** Returns the tile to display for the Google Photos collection. */
function getGooglePhotosTile(): ImageTile {
  return {
    name: loadTimeData.getString('googlePhotosLabel'),
    id: kGooglePhotosCollectionId,
    preview: [],
    type: TileType.IMAGE_GOOGLE_PHOTOS,
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
    if (isPngDataUrl(data)) {
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
    localImages: Array<FilePath|DefaultImageSymbol>,
    localImagesLoading: boolean,
    localImageData: Record<FilePath['path']|DefaultImageSymbol, Url>):
    ImageTile|LoadingTile {
  if (localImagesLoading) {
    return {type: TileType.LOADING};
  }

  const isMoreToLoad = localImages.some(
      image => !localImageData.hasOwnProperty(getPathOrSymbol(image)));

  const imagesToDisplay = getImages(localImages, localImageData);

  if (imagesToDisplay.length < kMaximumLocalImagePreviews && isMoreToLoad) {
    // If there are more images to attempt loading thumbnails for, wait until
    // those are done.
    return {type: TileType.LOADING};
  }

  // Count all images that failed to load and subtract them from "My Images"
  // count.
  const failureCount = Object.values(localImageData).reduce((result, next) => {
    return !isPngDataUrl(next) ? result + 1 : result;
  }, 0);

  return {
    name: loadTimeData.getString('myImagesLabel'),
    id: kLocalCollectionId,
    count: getCountText(
        Array.isArray(localImages) ? localImages.length - failureCount : 0),
    preview: imagesToDisplay,
    type: TileType.IMAGE_LOCAL,
  };
}

export class CollectionsGrid extends WithPersonalizationStore {
  static get is() {
    return 'collections-grid';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
      localImageData_: Object,

      /**
       * List of tiles to be displayed to the user.
       */
      tiles_: {
        type: Array,
        value() {
          // Fill the view with loading tiles. Will be adjusted to the correct
          // number of tiles when collections are received.
          return getLoadingPlaceholders(() => ({type: TileType.LOADING}));
        },
      },

      loadedCollectionIdPhotos_: {
        type: Set,
        value() {
          return new Set<string>();
        },
      },
    };
  }

  private collections_: WallpaperCollection[]|null;
  private images_: Record<string, WallpaperImage[]|null>;
  private imagesLoading_: Record<string, boolean>;
  private imageCounts_: Record<string, number|null>;
  private googlePhotosEnabled_: GooglePhotosEnablementState|undefined;
  private localImages_: Array<FilePath|DefaultImageSymbol>|null;
  private localImagesLoading_: boolean;
  private localImageData_: Record<string|DefaultImageSymbol, Url>;
  private tiles_: Tile[];
  private loadedCollectionIdPhotos_: Set<string>;

  static get observers() {
    return [
      'onLocalImagesChanged_(localImages_, localImagesLoading_, localImageData_)',
      'onCollectionLoaded_(collections_, imageCounts_)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<CollectionsGrid['collections_']>(
        'collections_', state => state.wallpaper.backdrop.collections);
    this.watch<CollectionsGrid['images_']>(
        'images_', state => state.wallpaper.backdrop.images);
    this.watch<CollectionsGrid['imagesLoading_']>(
        'imagesLoading_', state => state.wallpaper.loading.images);
    this.watch<CollectionsGrid['googlePhotosEnabled_']>(
        'googlePhotosEnabled_', state => state.wallpaper.googlePhotos.enabled);
    this.watch<CollectionsGrid['localImages_']>(
        'localImages_', state => state.wallpaper.local.images);
    // Treat as loading if either loading local images list or loading the
    // default image thumbnail. This prevents rapid churning of the UI on first
    // load.
    this.watch<CollectionsGrid['localImagesLoading_']>(
        'localImagesLoading_',
        state => state.wallpaper.loading.local.images ||
            state.wallpaper.loading.local.data[kDefaultImageSymbol]);
    this.watch<CollectionsGrid['localImageData_']>(
        'localImageData_', state => state.wallpaper.local.data);
    this.updateFromStore();
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

  getLoadingPlaceholderAnimationDelay(index: number): string {
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
    const offset =
        loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ? 2 : 1;

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

    const isDarkLightModeEnabled =
        loadTimeData.getBoolean('isDarkLightModeEnabled');

    collections.forEach((collection, i) => {
      const index = i + offset;
      const tile = this.tiles_[index];
      // This tile failed to load completely.
      if (imageCounts[collection.id] === null && !this.isFailureTile_(tile)) {
        this.set(`tiles_.${index}`, {
          id: collection.id,
          name: collection.name,
          count: '',
          preview: [],
          type: TileType.FAILURE,
        });
        return;
      }
      // This tile loaded successfully.
      if (typeof imageCounts[collection.id] === 'number' &&
          !this.isImageTile_(tile)) {
        this.set(`tiles_.${index}`, {
          id: collection.id,
          name: collection.name,
          count: getCountText(imageCounts[collection.id]),
          // Return all the previews in D/L mode to display the split view.
          // Otherwise, only the first preview is needed.
          preview: isNonEmptyArray(collection.previews) ?
              isDarkLightModeEnabled ? collection.previews :
                                       [collection.previews[0]] :
              [],
          type: TileType.IMAGE_ONLINE,
        });
      }
    });
  }

  /** Invoked on changes to |googlePhotosEnabled_|. */
  private onGooglePhotosEnabledChanged_(
      googlePhotosEnabled: CollectionsGrid['googlePhotosEnabled_']) {
    if (googlePhotosEnabled !== undefined) {
      const tile = getGooglePhotosTile();
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
    if (!Array.isArray(localImages) || !localImageData) {
      return;
    }
    const tile = getLocalTile(localImages, localImagesLoading, localImageData);
    this.set('tiles_.0', tile);
  }

  private getClassForImagesContainer_(tile: ImageTile): string {
    if (tile.type === TileType.IMAGE_ONLINE) {
      // Only apply base class for online collections.
      return 'photo-images-container';
    }
    const numImages =
        !!tile && Array.isArray(tile.preview) ? tile.preview.length : 0;
    return `photo-images-container photo-images-container-${
        Math.min(numImages, kMaximumLocalImagePreviews)}`;
  }

  /** Apply custom class for <img> to show a split view. */
  private getClassForImg_(index: number, tile: ImageTile): string {
    if (tile.type !== TileType.IMAGE_ONLINE || tile.preview.length < 2) {
      return '';
    }
    switch (index) {
      case 0:
        return 'left';
      case 1:
        return 'right';
      default:
        return '';
    }
  }

  private getClassForEmptyTile_(tile: ImageTile): string {
    return `photo-inner-container ${
        (this.isGooglePhotosTile_(tile) ? 'google-photos-empty' :
                                          'photo-empty')}`;
  }

  private getImageUrlForEmptyTile_(tile: ImageTile): string {
    return `chrome://personalization/images/${
        (this.isGooglePhotosTile_(tile) ? 'google_photos.svg' :
                                          'no_images.svg')}`;
  }

  /** Navigate to the correct route based on user selection. */
  private onCollectionSelected_(e: RepeaterEvent) {
    const tile = e.model.item;
    if (!isSelectionEvent(e) || !this.isSelectableTile_(tile)) {
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

  /**
   * Not using I18nBehavior because of chrome-untrusted:// incompatibility.
   * TODO(b:237323063) switch back to I18nBehavior.
   */
  private geti18n_(str: string): string {
    return loadTimeData.getString(str);
  }

  private isLoadingTile_(item: Tile|null): item is LoadingTile {
    return !!item && item.type === TileType.LOADING;
  }

  private isFailureTile_(item: Tile|null): item is FailureTile {
    return !!item && item.type === TileType.FAILURE;
  }

  private isTileTypeImage_(item: Tile|null): item is ImageTile {
    return !!item &&
        (item.type === TileType.IMAGE_GOOGLE_PHOTOS ||
         item.type === TileType.IMAGE_LOCAL ||
         item.type === TileType.IMAGE_ONLINE);
  }

  private isEmptyTile_(item: Tile|null): item is ImageTile {
    return this.isTileTypeImage_(item) && item.preview.length === 0;
  }

  private isGooglePhotosTile_(item: Tile|null): item is ImageTile|FailureTile {
    return !!item && (item.type === TileType.IMAGE_GOOGLE_PHOTOS) &&
        (item.id === kGooglePhotosCollectionId);
  }

  private isImageTile_(item: Tile|null): item is ImageTile {
    return this.isTileTypeImage_(item) && !this.isEmptyTile_(item);
  }

  private isManagedTile_(item: Tile|null): boolean {
    return this.isGooglePhotosTile_(item) &&
        this.googlePhotosEnabled_ === GooglePhotosEnablementState.kDisabled;
  }

  private isSelectableTile_(item: Tile|null): item is ImageTile|FailureTile {
    return (this.isGooglePhotosTile_(item) && !this.isManagedTile_(item)) ||
        this.isImageTile_(item);
  }

  private getTileAriaDisabled_(item: Tile|null): string {
    return (!this.isSelectableTile_(item)).toString();
  }

  private isPhotoTextHidden_(
      item: ImageTile, loadedCollectionIdPhotos: Set<string>): boolean {
    // Hide text until the first preview image for this collection has notified
    // that it finished loading.
    return !loadedCollectionIdPhotos.has(item.id);
  }

  /**
   * Make the text and background gradient visible again after the image has
   * finished loading. This is called for both on-load and on-error, as either
   * event should make the text visible again.
   */
  private onImgLoad_(event: Event) {
    const self = event.currentTarget! as HTMLElement;
    const collectionId = self.dataset['collectionId'];
    assert(
        collectionId &&
            ((this.collections_ ||
              []).some(collection => collection.id === collectionId) ||
             collectionId === kLocalCollectionId ||
             collectionId === kGooglePhotosCollectionId),
        'valid collection id required');
    if (!this.loadedCollectionIdPhotos_.has(collectionId)) {
      this.loadedCollectionIdPhotos_ =
          new Set([...this.loadedCollectionIdPhotos_, collectionId]);
    }
  }

  private getAriaIndex_(index: number): number {
    return index + 1;
  }
}

customElements.define(CollectionsGrid.is, CollectionsGrid);
