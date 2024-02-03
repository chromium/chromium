// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays the Google Photos
 * collection.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './google_photos_zero_state_element.js';

import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperProviderInterface} from '../../personalization_app.mojom-webui.js';
import {isGooglePhotosSharedAlbumsEnabled} from '../load_time_booleans.js';
import {Paths, PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './google_photos_collection_element.html.js';
import {fetchGooglePhotosAlbums, fetchGooglePhotosPhotos, fetchGooglePhotosSharedAlbums} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

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

export interface GooglePhotosCollectionElement {
  $: {main: HTMLElement};
}

export class GooglePhotosCollectionElement extends WithPersonalizationStore {
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

  override ready() {
    super.ready();
    afterNextRender(this, () => {
      this.$.main.focus();
    });
  }

  override connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosCollectionElement['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosCollectionElement['albumsLoading_']>(
        'albumsLoading_', state => state.wallpaper.loading.googlePhotos.albums);
    if (this.isSharedAlbumsEnabled_) {
      this.watch<GooglePhotosCollectionElement['albumsShared_']>(
          'albumsShared_', state => state.wallpaper.googlePhotos.albumsShared);
      this.watch<GooglePhotosCollectionElement['albumsSharedLoading_']>(
          'albumsSharedLoading_',
          state => state.wallpaper.loading.googlePhotos.albumsShared);
    }
    this.watch<GooglePhotosCollectionElement['enabled_']>(
        'enabled_', state => state.wallpaper.googlePhotos.enabled);
    this.watch<GooglePhotosCollectionElement['photos_']>(
        'photos_', state => state.wallpaper.googlePhotos.photos);
    this.watch<GooglePhotosCollectionElement['photosByAlbumId_']>(
        'photosByAlbumId_',
        state => state.wallpaper.googlePhotos.photosByAlbumId);
    this.watch<GooglePhotosCollectionElement['photosLoading_']>(
        'photosLoading_', state => state.wallpaper.loading.googlePhotos.photos);

    this.updateFromStore();
  }

  /** Invoked on changes to the currently selected |albumId|. */
  private onAlbumIdChanged_(albumId: GooglePhotosCollectionElement['albumId']) {
    this.tab_ =
        albumId ? GooglePhotosTab.PHOTOS_BY_ALBUM_ID : GooglePhotosTab.ALBUMS;
  }

  /** Invoked on changes to either |path| or |enabled_|. */
  private onPathOrEnabledChanged_(
      path: GooglePhotosCollectionElement['path'],
      enabled: GooglePhotosCollectionElement['enabled_']) {
    // If the Google Photos collection is selected but the user is not allowed
    // to access Google Photos, redirect back to the collections page.
    if (path === Paths.GOOGLE_PHOTOS_COLLECTION &&
        enabled === GooglePhotosEnablementState.kDisabled) {
      PersonalizationRouterElement.reloadAtWallpaper();
    }

    if (enabled === GooglePhotosEnablementState.kEnabled) {
      // When the user first selects the Google Photos collection it should
      // result in a data fetch for the user's photos.
      if (this.photos_ === undefined && !this.photosLoading_) {
        fetchGooglePhotosPhotos(this.wallpaperProvider_, this.getStore());
      }

      // When the user first selects the Google Photos collection it should
      // result in a data fetch for the user's albums.
      if (this.albums_ === undefined && !this.albumsLoading_ &&
          this.albumsShared_ === undefined && !this.albumsSharedLoading_) {
        fetchGooglePhotosAlbums(this.wallpaperProvider_, this.getStore());
        if (this.isSharedAlbumsEnabled_) {
          fetchGooglePhotosSharedAlbums(
              this.wallpaperProvider_, this.getStore());
        }
      }
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
      albums: GooglePhotosCollectionElement['albums_'],
      albumsShared: GooglePhotosCollectionElement['albumsShared_']): boolean {
    if (this.isSharedAlbumsEnabled_) {
      // The list of (owned+shared) albums is empty only if both albums are
      // enpty.
      return !isNonEmptyArray(albums) && !isNonEmptyArray(albumsShared);
    }
    return !isNonEmptyArray(albums);
  }

  /** Whether the albums tab is currently selected. */
  private isAlbumsTabSelected_(tab: GooglePhotosCollectionElement['tab_']):
      boolean {
    return tab === GooglePhotosTab.ALBUMS;
  }

  /** Whether the albums tab content is currently visible. */
  private isAlbumsTabContentVisible_(
      tab: GooglePhotosCollectionElement['tab_']): boolean {
    return this.isAlbumsTabSelected_(tab);
  }

  /** Whether the photos by album id tab is currently selected. */
  private isPhotosByAlbumIdTabSelected_(
      tab: GooglePhotosCollectionElement['tab_']): boolean {
    return tab === GooglePhotosTab.PHOTOS_BY_ALBUM_ID;
  }

  /** Whether the photos by album id tab content is currently visible. */
  private isPhotosByAlbumIdTabContentVisible_(
      albumId: GooglePhotosCollectionElement['albumId'],
      photosByAlbumId: GooglePhotosCollectionElement['photosByAlbumId_'],
      tab: GooglePhotosCollectionElement['tab_']): boolean {
    return this.isPhotosByAlbumIdTabSelected_(tab) &&
        !this.isPhotosByAlbumIdTabZeroStateVisible_(
            albumId, photosByAlbumId, tab);
  }

  /** Whether the photos by album id tab zero state is currently visible. */
  private isPhotosByAlbumIdTabZeroStateVisible_(
      albumId: GooglePhotosCollectionElement['albumId'],
      photosByAlbumId: GooglePhotosCollectionElement['photosByAlbumId_'],
      tab: GooglePhotosCollectionElement['tab_']): boolean {
    return this.isPhotosByAlbumIdTabSelected_(tab) && !!albumId &&
        !!photosByAlbumId && isEmptyArray(photosByAlbumId[albumId]);
  }

  /** Whether the photos tab is currently selected. */
  private isPhotosTabSelected_(tab: GooglePhotosCollectionElement['tab_']):
      boolean {
    return tab === GooglePhotosTab.PHOTOS;
  }

  /** Whether the photos tab content is currently visible. */
  private isPhotosTabContentVisible_(
      photos: GooglePhotosCollectionElement['photos_'],
      tab: GooglePhotosCollectionElement['tab_']): boolean {
    return this.isPhotosTabSelected_(tab) &&
        !this.isPhotosTabZeroStateVisible_(photos, tab);
  }

  /** Whether the photos tab zero state is currently visible. */
  private isPhotosTabZeroStateVisible_(
      photos: GooglePhotosCollectionElement['photos_'],
      tab: GooglePhotosCollectionElement['tab_']): boolean {
    return this.isPhotosTabSelected_(tab) && isEmptyArray(photos);
  }

  /** Whether the tab strip is currently visible. */
  private isTabStripVisible_(
      albumId: GooglePhotosCollectionElement['albumId'],
      albums: GooglePhotosCollectionElement['albums_'],
      albumsShared: GooglePhotosCollectionElement['albumsShared_']): boolean {
    return !albumId && !this.isAlbumsEmpty_(albums, albumsShared);
  }

  /** Whether zero state is currently visible. */
  private isZeroStateVisible_(
      albumId: GooglePhotosCollectionElement['albumId'],
      photos: GooglePhotosCollectionElement['photos_'],
      photosByAlbumId: GooglePhotosCollectionElement['photosByAlbumId_'],
      tab: GooglePhotosCollectionElement['tab_']): boolean {
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

customElements.define(
    GooglePhotosCollectionElement.is, GooglePhotosCollectionElement);
