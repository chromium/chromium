// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './setup.js';
import '../trusted/wallpaper/styles.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {DefaultImageSymbol, Events, EventType, kMaximumLocalImagePreviews} from '../common/constants.js';
import {getCountText, getLoadingPlaceholderAnimationDelay, getLoadingPlaceholders, isNonEmptyArray, isSelectionEvent} from '../common/utils.js';
import {GooglePhotosEnablementState, WallpaperCollection} from '../trusted/personalization_app.mojom-webui.js';
import {getPathOrSymbol} from '../trusted/utils.js';

import {getTemplate} from './collections_grid.html.js';
import {selectCollection, selectGooglePhotosCollection, selectLocalCollection, validateReceivedData} from './iframe_api.js';

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

type LoadingTile = {
  type: TileType.LOADING,
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
  type: TileType.IMAGE_GOOGLE_PHOTOS|TileType.IMAGE_LOCAL|TileType.IMAGE_ONLINE,
  id: string,
  name: string,
  count?: string, preview: Url[],
};

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
    localImageData: Record<string|DefaultImageSymbol, string>): Url[] {
  if (!localImageData || !Array.isArray(localImages)) {
    return [];
  }
  const result = [];
  for (const image of localImages) {
    const key = getPathOrSymbol(image);
    const data = {url: localImageData[key]};
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
    localImages: Array<FilePath|DefaultImageSymbol>,
    localImageData: Record<FilePath['path']|DefaultImageSymbol, string>):
    ImageTile|LoadingTile {
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
    return next === '' ? result + 1 : result;
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

export class CollectionsGrid extends PolymerElement {
  static get is() {
    return 'collections-grid';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      collections_: Array,

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
          return getLoadingPlaceholders(() => ({type: TileType.LOADING}));
        }
      },
    };
  }

  private collections_: WallpaperCollection[];
  private googlePhotosEnabled_: GooglePhotosEnablementState|undefined;
  private imageCounts_: {[key: string]: number|null};
  private localImages_: Array<FilePath|DefaultImageSymbol>;
  private localImageData_: {[key: string]: string};
  private tiles_: Tile[];

  static get observers() {
    return [
      'onLocalImagesLoaded_(localImages_, localImageData_)',
      'onCollectionLoaded_(collections_, imageCounts_)',
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
  private onLocalImagesLoaded_(
      localImages: Array<FilePath|DefaultImageSymbol>|undefined,
      localImageData: Record<FilePath['path']|DefaultImageSymbol, string>) {
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
      case EventType.SEND_GOOGLE_PHOTOS_ENABLED:
        if (isValid) {
          this.googlePhotosEnabled_ = event.enabled;
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
