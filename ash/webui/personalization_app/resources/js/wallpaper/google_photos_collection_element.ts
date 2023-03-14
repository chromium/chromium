// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays the Google Photos
 * collection.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../../css/wallpaper.css.js';
import '../../css/common.css.js';
import './google_photos_zero_state_element.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperProviderInterface} from '../../personalization_app.mojom-webui.js';
import {isGooglePhotosSharedAlbumsEnabled} from '../load_time_booleans.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isNonEmptyArray} from '../utils.js';

import {getTemplate} from './google_photos_collection_element.html.js';
import {fetchGooglePhotosAlbums, fetchGooglePhotosPhotos, fetchGooglePhotosSharedAlbums, initializeGooglePhotosData} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

/** A Promise<T> which can be externally |resolve()|-ed. */
type ExternallyResolvablePromise<T> = Promise<T>&{resolve: (result: T) => void};

/** Creates a Promise<T> which can be externally |resolve()|-ed. */
function createExternallyResolvablePromise<T>():
    ExternallyResolvablePromise<T> {
  let externalResolver: (result: T) => void;
  const promise = new Promise<T>(resolve => {
                    externalResolver = resolve;
                  }) as ExternallyResolvablePromise<T>;
  promise.resolve = externalResolver!;
  return promise;
}

/**
 * Checks if argument is an array with zero length.
 */
function isEmptyArray(maybeArray: unknown): maybeArray is[] {
  return Array.isArray(maybeArray) && maybeArray.length === 0;
}

/** Enumeration of supported tabs. */
export enum GooglePhotosTab {
  ALBUMS = 'albums',
  PHOTOS = 'photos',
  PHOTOS_BY_ALBUM_ID = 'photos_by_album_id',
}

export interface GooglePhotosCollection {
  $: {main: HTMLElement};
}

export class GooglePhotosCollection extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-collection';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      albumId: {
        type: String,
        observer: 'onAlbumIdChanged_',
      },

      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      path: String,

      albums_: Array,
      albumsLoading_: Boolean,
      albumsShared_: Array,
      albumsSharedLoading_: Boolean,
      enabled_: Number,
      photos_: Array,
      photosByAlbumId_: Object,

      tab_: {
        type: String,
        value: GooglePhotosTab.PHOTOS,
      },

      isSharedAlbumsEnabled_: {
        type: Boolean,
        value() {
          return isGooglePhotosSharedAlbumsEnabled();
        },
      },
    };
  }

  static get observers() {
    return ['onPathOrEnabledChanged_(path, enabled_)'];
  }

  /** The currently selected album id. */
  albumId: string|undefined;

  /** Whether or not this element is currently hidden. */
  override hidden: boolean;

  /** The currently selected path. */
  path: string|undefined;

  /** The list of owned albums. */
  private albums_: GooglePhotosAlbum[]|null|undefined;

  /** Whether the list of owned albums is currently loading. */
  private albumsLoading_: boolean|undefined;

  /** The list of shared albums. */
  private albumsShared_: GooglePhotosAlbum[]|null|undefined;

  /** Whether the list of shared albums is currently loading. */
  private albumsSharedLoading_: boolean|undefined;

  /** Whether the user is allowed to access Google Photos. */
  private enabled_: GooglePhotosEnablementState|undefined;

  /**
   * Promise which is resolved after initializing Google Photos data. Note that
   * this promise is created early (instead of at data initialization request
   * time) so that it can be waited on prior to the request.
   */
  private initializeGooglePhotosDataPromise_:
      ExternallyResolvablePromise<void> =
          createExternallyResolvablePromise<void>();

  /** The list of photos. */
  private photos_: GooglePhotosPhoto[]|null|undefined;

  /** The list of photos by album id. */
  private photosByAlbumId_: Record<string, GooglePhotosPhoto[]|null|undefined>|
      undefined;

  /** Whether the list of photos is currently loading. */
  private photosLoading_: boolean|undefined;

  /** The currently selected tab. */
  private tab_: GooglePhotosTab;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  /** Whether feature flag |kGooglePhotosSharedAlbums| is enabled. */
  private isSharedAlbumsEnabled_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosCollection['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosCollection['albumsLoading_']>(
        'albumsLoading_', state => state.wallpaper.loading.googlePhotos.albums);
    if (this.isSharedAlbumsEnabled_) {
      this.watch<GooglePhotosCollection['albumsShared_']>(
          'albumsShared_', state => state.wallpaper.googlePhotos.albumsShared);
      this.watch<GooglePhotosCollection['albumsSharedLoading_']>(
          'albumsSharedLoading_',
          state => state.wallpaper.loading.googlePhotos.albumsShared);
    }
    this.watch<GooglePhotosCollection['enabled_']>(
        'enabled_', state => state.wallpaper.googlePhotos.enabled);
    this.watch<GooglePhotosCollection['photos_']>(
        'photos_', state => state.wallpaper.googlePhotos.photos);
    this.watch<GooglePhotosCollection['photosByAlbumId_']>(
        'photosByAlbumId_',
        state => state.wallpaper.googlePhotos.photosByAlbumId);
    this.watch<GooglePhotosCollection['photosLoading_']>(
        'photosLoading_', state => state.wallpaper.loading.googlePhotos.photos);

    this.updateFromStore();

    initializeGooglePhotosData(this.wallpaperProvider_, this.getStore())
        .then(() => this.initializeGooglePhotosDataPromise_.resolve());
  }

  /** Invoked on changes to the currently selected |albumId|. */
  private onAlbumIdChanged_(albumId: GooglePhotosCollection['albumId']) {
    this.tab_ =
        albumId ? GooglePhotosTab.PHOTOS_BY_ALBUM_ID : GooglePhotosTab.ALBUMS;
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_(hidden: GooglePhotosCollection['hidden']) {
    if (hidden) {
      return;
    }

    document.title = this.i18n('googlePhotosLabel');
    this.$.main.focus();

    // When the user first selects the Google Photos collection it should result
    // in a data fetch for the user's photos.
    if (this.photos_ === undefined && !this.photosLoading_) {
      this.initializeGooglePhotosDataPromise_.then(() => {
        fetchGooglePhotosPhotos(this.wallpaperProvider_, this.getStore());
      });
    }

    // When the user first selects the Google Photos collection it should result
    // in a data fetch for the user's albums.
    if (this.albums_ === undefined && !this.albumsLoading_ &&
        this.albumsShared_ === undefined && !this.albumsSharedLoading_) {
      this.initializeGooglePhotosDataPromise_.then(() => {
        fetchGooglePhotosAlbums(this.wallpaperProvider_, this.getStore());
        if (this.isSharedAlbumsEnabled_) {
          fetchGooglePhotosSharedAlbums(
              this.wallpaperProvider_, this.getStore());
        }
      });
    }
  }

  /** Invoked on changes to either |path| or |enabled_|. */
  private onPathOrEnabledChanged_(
      path: GooglePhotosCollection['path'],
      enabled: GooglePhotosCollection['enabled_']) {
    // If the Google Photos collection is selected but the user is not allowed
    // to access Google Photos, redirect back to the collections page.
    if (path === Paths.GOOGLE_PHOTOS_COLLECTION &&
        enabled === GooglePhotosEnablementState.kDisabled) {
      PersonalizationRouter.reloadAtWallpaper();
    }
  }

  /** Invoked on tab selected. */
  private onTabSelected_(e: Event) {
    const currentTarget: HTMLElement = e.currentTarget as HTMLElement;
    switch (currentTarget.id) {
      case 'albumsTab':
        this.tab_ = GooglePhotosTab.ALBUMS;
        return;
      case 'photosTab':
        this.tab_ = GooglePhotosTab.PHOTOS;
        return;
      default:
        assertNotReached();
    }
  }

  /** Whether the list of albums is empty. */
  private isAlbumsEmpty_(
      albums: GooglePhotosCollection['albums_'],
      albumsShared: GooglePhotosCollection['albumsShared_']): boolean {
    if (this.isSharedAlbumsEnabled_) {
      // The list of (owned+shared) albums is empty only if both albums are
      // enpty.
      return !isNonEmptyArray(albums) && !isNonEmptyArray(albumsShared);
    }
    return !isNonEmptyArray(albums);
  }

  /** Whether the albums tab is currently selected. */
  private isAlbumsTabSelected_(tab: GooglePhotosCollection['tab_']): boolean {
    return tab === GooglePhotosTab.ALBUMS;
  }

  /** Whether the albums tab content is currently visible. */
  private isAlbumsTabContentVisible_(
      hidden: GooglePhotosCollection['hidden'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return this.isAlbumsTabSelected_(tab) && !hidden;
  }

  /** Whether the photos by album id tab is currently selected. */
  private isPhotosByAlbumIdTabSelected_(tab: GooglePhotosCollection['tab_']):
      boolean {
    return tab === GooglePhotosTab.PHOTOS_BY_ALBUM_ID;
  }

  /** Whether the photos by album id tab content is currently visible. */
  private isPhotosByAlbumIdTabContentVisible_(
      albumId: GooglePhotosCollection['albumId'],
      hidden: GooglePhotosCollection['hidden'],
      photosByAlbumId: GooglePhotosCollection['photosByAlbumId_'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return this.isPhotosByAlbumIdTabSelected_(tab) &&
        !this.isPhotosByAlbumIdTabZeroStateVisible_(
            albumId, photosByAlbumId, tab) &&
        !hidden;
  }

  /** Whether the photos by album id tab zero state is currently visible. */
  private isPhotosByAlbumIdTabZeroStateVisible_(
      albumId: GooglePhotosCollection['albumId'],
      photosByAlbumId: GooglePhotosCollection['photosByAlbumId_'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return this.isPhotosByAlbumIdTabSelected_(tab) && !!albumId &&
        !!photosByAlbumId && isEmptyArray(photosByAlbumId[albumId]);
  }

  /** Whether the photos tab is currently selected. */
  private isPhotosTabSelected_(tab: GooglePhotosCollection['tab_']): boolean {
    return tab === GooglePhotosTab.PHOTOS;
  }

  /** Whether the photos tab content is currently visible. */
  private isPhotosTabContentVisible_(
      hidden: GooglePhotosCollection['hidden'],
      photos: GooglePhotosCollection['photos_'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return this.isPhotosTabSelected_(tab) &&
        !this.isPhotosTabZeroStateVisible_(photos, tab) && !hidden;
  }

  /** Whether the photos tab zero state is currently visible. */
  private isPhotosTabZeroStateVisible_(
      photos: GooglePhotosCollection['photos_'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return this.isPhotosTabSelected_(tab) && isEmptyArray(photos);
  }

  /** Whether the tab strip is currently visible. */
  private isTabStripVisible_(
      albumId: GooglePhotosCollection['albumId'],
      albums: GooglePhotosCollection['albums_'],
      albumsShared: GooglePhotosCollection['albumsShared_']): boolean {
    return !albumId && !this.isAlbumsEmpty_(albums, albumsShared);
  }

  /** Whether zero state is currently visible. */
  private isZeroStateVisible_(
      albumId: GooglePhotosCollection['albumId'],
      photos: GooglePhotosCollection['photos_'],
      photosByAlbumId: GooglePhotosCollection['photosByAlbumId_'],
      tab: GooglePhotosCollection['tab_']): boolean {
    switch (tab) {
      case GooglePhotosTab.ALBUMS:
        return false;
      case GooglePhotosTab.PHOTOS:
        return this.isPhotosTabZeroStateVisible_(photos, tab);
      case GooglePhotosTab.PHOTOS_BY_ALBUM_ID:
        return this.isPhotosByAlbumIdTabZeroStateVisible_(
            albumId, photosByAlbumId, tab);
      default:
        assertNotReached();
    }
  }
}

customElements.define(GooglePhotosCollection.is, GooglePhotosCollection);
