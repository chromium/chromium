// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './setup.js';
import '../trusted/wallpaper/styles.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {Events, EventType, kMaximumGooglePhotosPreviews, kMaximumLocalImagePreviews} from '../common/constants.js';
import {getCountText, getLoadingPlaceholderAnimationDelay, getNumberOfGridItemsPerRow, isNullOrArray, isNullOrNumber, isSelectionEvent} from '../common/utils.js';
import {WallpaperCollection} from '../trusted/personalization_app.mojom-webui.js';
import {selectCollection, selectGooglePhotosCollection, selectLocalCollection, validateReceivedData} from '../untrusted/iframe_api.js';

/**
 * @fileoverview Responds to |SendCollectionsEvent| from trusted. Handles user
 * input and responds with |SelectCollectionEvent| when an image is selected.
 */

const kGooglePhotosCollectionId = 'google_photos_';
const kLocalCollectionId = 'local_';

/** Height in pixels of a tile. */
const kTileHeightPx = 136;

enum TileType {
  LOADING = 'loading',
  IMAGE = 'image',
  FAILURE = 'failure',
}

type LoadingTile = {
  type: TileType.LOADING
};

/**
 * Type that represents a collection that failed to load. The preview image
 * is still displayed, but is grayed out and unclickable.
 */
type FailureTile = {
  type: TileType.FAILURE,
  id: string,
  name: string,
  preview: [],
};

/**
 * A displayable type constructed from up to three LocalImages or a
 * WallpaperCollection.
 */
type ImageTile = {
  type: TileType.IMAGE,
  id: string,
  name: string,
  count: string,
  preview: Url[],
};

type Tile = LoadingTile|FailureTile|ImageTile;

interface RepeaterEvent extends CustomEvent {
  model: {
    item: Tile,
  };
}

/** Returns the tile to display for the Google Photos collection. */
function getGooglePhotosTile(
    googlePhotos: Url[]|null, googlePhotosCount: number|null): ImageTile {
  return {
    name: loadTimeData.getString('googlePhotosLabel'),
    id: kGooglePhotosCollectionId,
    count: getCountText(googlePhotosCount ?? 0),
    preview: googlePhotos?.slice(0, kMaximumGooglePhotosPreviews) ?? [],
    type: TileType.IMAGE,
  };
}

function getImages(
    localImages: FilePath[], localImageData: Record<string, string>): Url[] {
  if (!localImageData || !Array.isArray(localImages)) {
    return [];
  }
  const result = [];
  for (const {path} of localImages) {
    const data = {url: localImageData[path]};
    if (!!data.url && data.url.length > 0) {
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
    localImages: FilePath[],
    localImageData: {[key: string]: string}): ImageTile|LoadingTile {
  const isMoreToLoad =
      localImages.some(({path}) => !localImageData.hasOwnProperty(path));

  const imagesToDisplay = getImages(localImages, localImageData);

  if (imagesToDisplay.length < kMaximumLocalImagePreviews && isMoreToLoad) {
    // If there are more images to attempt loading thumbnails for, wait until
    // those are done.
    return {type: TileType.LOADING};
  }

  // Count all images that failed to load and subtract them from "My Images"
  // count.
  const failureCount = Object.values(localImageData).reduce((result, next) => {
    return next === '' ? result + 1 : result;
  }, 0);

  return {
    name: loadTimeData.getString('myImagesLabel'),
    id: kLocalCollectionId,
    count: getCountText(
        Array.isArray(localImages) ? localImages.length - failureCount : 0),
    preview: imagesToDisplay,
    type: TileType.IMAGE,
  };
}

export class CollectionsGrid extends PolymerElement {
  static get is() {
    return 'collections-grid';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      collections_: Array,

      /**
       * The list of Google Photos photos.
       */
      googlePhotos_: Array,

      /**
       * The count of Google Photos photos.
       */
      googlePhotosCount_: Number,

      /**
       * Mapping of collection id to number of images. Loads in progressively
       * after collections_.
       */
      imageCounts_: Object,

      localImages_: Array,

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
          const x = getNumberOfGridItemsPerRow();
          const y = Math.floor(window.innerHeight / kTileHeightPx);
          return Array.from({length: x * y}, () => ({type: TileType.LOADING}));
        }
      },
    };
  }

  private collections_: WallpaperCollection[];
  private googlePhotos_: unknown[]|null;
  private googlePhotosCount_: number|null;
  private imageCounts_: {[key: string]: number|null};
  private localImages_: FilePath[];
  private localImageData_: {[key: string]: string};
  private tiles_: Tile[];

  static get observers() {
    return [
      'onLocalImagesLoaded_(localImages_, localImageData_)',
      'onCollectionLoaded_(collections_, imageCounts_)',
      'onGooglePhotosLoaded_(googlePhotos_, googlePhotosCount_)',
    ];
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
      imageCounts: {[key: string]: number|null}) {
    if (!Array.isArray(collections) || !imageCounts) {
      return;
    }

    // The first tile in the collections grid is reserved for local images. The
    // second tile is reserved for Google Photos, provided that the integration
    // is enabled. The tile index of other collections must be `offset` so as
    // not to occupy reserved space.
    const offset =
        loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ? 2 : 1;

    while (this.tiles_.length < collections.length + offset) {
      this.push('tiles_', {type: TileType.LOADING});
    }
    while (this.tiles_.length > collections.length + offset) {
      this.pop('tiles_');
    }

    collections.forEach((collection, i) => {
      const index = i + offset;
      const tile = this.tiles_[index];
      // This tile failed to load completely.
      if (imageCounts[collection.id] === null && !this.isFailureTile_(tile)) {
        this.set(`tiles_.${index}`, {
          id: collection.id,
          name: collection.name,
          count: '',
          preview: [collection.preview],
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
          preview: [collection.preview],
          type: TileType.IMAGE,
        });
      }
    });
  }

  /** Invoked on changes to the list and count of Google Photos photos. */
  private onGooglePhotosLoaded_(
      googlePhotos: Url[]|null|undefined,
      googlePhotosCount: number|null|undefined) {
    if (isNullOrArray(googlePhotos) && isNullOrNumber(googlePhotosCount)) {
      const tile = getGooglePhotosTile(googlePhotos, googlePhotosCount);
      this.set('tiles_.1', tile);
    }
  }

  /**
   * Called with updated local image list or local image thumbnail data when
   * either of those properties changes.
   */
  private onLocalImagesLoaded_(
      localImages: FilePath[]|undefined,
      localImageData: {[key: string]: string}) {
    if (!Array.isArray(localImages) || !localImageData) {
      return;
    }
    const tile = getLocalTile(localImages, localImageData);
    this.set('tiles_.0', tile);
  }

  /**
   * Handler for messages from trusted code.
   * TODO(cowmoo) move this up beneath static properties because it is public.
   */
  onMessageReceived(event: Events) {
    const isValid = validateReceivedData(event);
    if (!isValid) {
      console.warn('Invalid event message received, event type: ' + event.type);
    }

    switch (event.type) {
      case EventType.SEND_COLLECTIONS:
        this.collections_ = isValid ? event.collections : [];
        break;
      case EventType.SEND_GOOGLE_PHOTOS_COUNT:
        if (isValid) {
          this.googlePhotosCount_ = event.count;
        } else {
          this.googlePhotos_ = null;
          this.googlePhotosCount_ = null;
        }
        break;
      case EventType.SEND_GOOGLE_PHOTOS_PHOTOS:
        if (isValid) {
          this.googlePhotos_ = event.photos;
        } else {
          this.googlePhotos_ = null;
          this.googlePhotosCount_ = null;
        }
        break;
      case EventType.SEND_IMAGE_COUNTS:
        this.imageCounts_ = event.counts;
        break;
      case EventType.SEND_LOCAL_IMAGES:
        if (isValid) {
          this.localImages_ = event.images;
        } else {
          this.localImages_ = [];
          this.localImageData_ = {};
        }
        break;
      case EventType.SEND_LOCAL_IMAGE_DATA:
        if (isValid) {
          this.localImageData_ = event.data;
        } else {
          this.localImages_ = [];
          this.localImageData_ = {};
        }
        break;
      case EventType.SEND_VISIBLE:
        if (!isValid) {
          return;
        }

        const visible = event.visible;
        if (visible) {
          // If iron-list items were updated while this iron-list was hidden,
          // the layout will be incorrect. Trigger another layout when iron-list
          // becomes visible again. Wait until |afterNextRender| completes
          // otherwise iron-list width may still be 0.
          afterNextRender(this, () => {
            // Trigger a layout now that iron-list has the correct width.
            this.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
          });
        }
        return;
      default:
        console.error(`Unexpected event type ${event.type}`);
        break;
    }
  }

  private getClassForImagesContainer_(tile: ImageTile): string {
    const numImages = Array.isArray(tile?.preview) ? tile.preview.length : 0;
    return `photo-images-container photo-images-container-${
        Math.min(numImages, kMaximumLocalImagePreviews)}`;
  }

  getClassForEmptyTile_(tile: ImageTile): string {
    return `photo-inner-container ${
        (this.isGooglePhotosTile_(tile) ? 'google-photos-empty' :
                                          'photo-empty')}`;
  }

  getImageUrlForEmptyTile_(tile: ImageTile): string {
    return `chrome://personalization/common/${
        (this.isGooglePhotosTile_(tile) ? 'google_photos.svg' :
                                          'no_images.svg')}`;
  }

  /**
   * Notify trusted code that a user selected a collection.
   */
  private onCollectionSelected_(e: RepeaterEvent) {
    const tile = e.model.item;
    if (!isSelectionEvent(e) || !this.isSelectableTile_(tile)) {
      return;
    }
    switch (tile.id) {
      case kGooglePhotosCollectionId:
        selectGooglePhotosCollection();
        return;
      case kLocalCollectionId:
        selectLocalCollection();
        return;
      default:
        selectCollection(tile.id);
        return;
    }
  }

  /**
   * Not using I18nBehavior because of chrome-untrusted:// incompatibility.
   */
  private geti18n_(str: string): string {
    return loadTimeData.getString(str);
  }

  private isLoadingTile_(item: Tile|null): item is LoadingTile {
    return item?.type === TileType.LOADING;
  }

  private isFailureTile_(item: Tile|null): item is FailureTile {
    return item?.type === TileType.FAILURE;
  }

  private isEmptyTile_(item: Tile|null): item is ImageTile {
    return !!item && item.type === TileType.IMAGE && item.preview.length === 0;
  }

  private isGooglePhotosTile_(item: Tile|null): item is ImageTile|FailureTile {
    return !!item && (item.type !== TileType.LOADING) &&
        (item?.id === kGooglePhotosCollectionId);
  }

  private isImageTile_(item: Tile|null): item is ImageTile {
    return item?.type === TileType.IMAGE && !this.isEmptyTile_(item);
  }

  private isSelectableTile_(item: Tile|null): item is ImageTile|FailureTile {
    return this.isGooglePhotosTile_(item) || this.isImageTile_(item);
  }

  /**
   * Make the text and background gradient visible again after the image has
   * finished loading. This is called for both on-load and on-error, as either
   * event should make the text visible again.
   */
  private onImgLoad_(event: Event) {
    const self = event.currentTarget! as HTMLElement;
    const parent = self.closest('.photo-inner-container');
    for (const elem of parent!.querySelectorAll('[hidden]')) {
      elem.removeAttribute('hidden');
    }
  }
}

customElements.define(CollectionsGrid.is, CollectionsGrid);
