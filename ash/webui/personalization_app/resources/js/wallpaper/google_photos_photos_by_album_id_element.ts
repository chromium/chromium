// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays Google Photos photos
 * for the currently selected album id.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';

import {WallpaperGridItemSelectedEvent} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {assert} from 'chrome://resources/js/assert.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CurrentWallpaper, GooglePhotosAlbum, GooglePhotosPhoto, WallpaperProviderInterface, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {isGooglePhotosSharedAlbumsEnabled} from '../load_time_booleans.js';
import {dismissErrorAction, setErrorAction} from '../personalization_actions.js';
import {PersonalizationStateError} from '../personalization_state.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {DisplayableImage} from './constants.js';
import {recordWallpaperGooglePhotosSourceUMA, WallpaperGooglePhotosSource} from './google_photos_metrics_logger.js';
import {getTemplate} from './google_photos_photos_by_album_id_element.html.js';
import {findAlbumById, getLoadingPlaceholders, isGooglePhotosPhoto, isImageAMatchForKey, isImageEqualToSelected} from './utils.js';
import {fetchGooglePhotosAlbum, selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const ERROR_ID = 'GooglePhotosByAlbumId';
const PLACEHOLDER_ID = 'placeholder';

/** Returns placeholders to show while Google Photos photos are loading. */
function getPlaceholders(): GooglePhotosPhoto[] {
  return getLoadingPlaceholders(() => {
    return {
      id: PLACEHOLDER_ID,
      name: '',
      date: {data: []},
      url: {url: ''},
      dedupKey: null,
      location: null,
    };
  });
}

export interface GooglePhotosPhotosByAlbumIdElement {
  $: {grid: IronListElement, gridScrollThreshold: IronScrollThresholdElement};
}

export class GooglePhotosPhotosByAlbumIdElement extends
    WithPersonalizationStore {
  static get is() {
    return 'google-photos-photos-by-album-id';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      albumId: String,

      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      album_: {
        type: Array,
        value: getPlaceholders,
      },

      albums_: Array,
      albumsShared_: Array,
      currentSelected_: Object,
      pendingSelected_: Object,
      photosByAlbumId_: Object,
      photosByAlbumIdLoading_: Object,
      photosByAlbumIdResumeTokens_: Object,

      isSharedAlbumsEnabled_: {
        type: Boolean,
        value() {
          return isGooglePhotosSharedAlbumsEnabled();
        },
      },

      error_: {
        type: Object,
        value: null,
      },
    };
  }

  static get observers() {
    return [
      'onAlbumIdOrAlbumsOrPhotosByAlbumIdChanged_(albumId, albums_, albumsShared_, photosByAlbumId_)',
      'onAlbumIdOrPhotosByAlbumIdResumeTokensChanged_(albumId, photosByAlbumIdResumeTokens_)',
    ];
  }

  /** The currently selected album id. */
  albumId: string|undefined;

  /** Whether or not this element is currently hidden. */
  override hidden: boolean;

  /** The list of photos for the currently selected album id. */
  private album_: GooglePhotosPhoto[];

  /** The list of Google Photos albums. */
  private albums_: GooglePhotosAlbum[]|null|undefined;

  /** The list of shared Google Photos albums. */
  private albumsShared_: GooglePhotosAlbum[]|null|undefined;

  /** The currently selected wallpaper. */
  private currentSelected_: CurrentWallpaper|null;

  /** The pending selected wallpaper. */
  private pendingSelected_: DisplayableImage|null;

  /** The list of photos by album id. */
  private photosByAlbumId_: Record<string, GooglePhotosPhoto[]|null|undefined>|
      undefined;

  /** Whether the list of photos by album id is currently loading. */
  private photosByAlbumIdLoading_: Record<string, boolean>|undefined;

  /** The resume tokens needed to fetch the next page of photos by album id. */
  private photosByAlbumIdResumeTokens_: Record<string, string|null>|undefined;

  /** Whether feature flag |kGooglePhotosSharedAlbums| is enabled. */
  private isSharedAlbumsEnabled_: boolean;

  /** The current personalization error state. */
  private error_: PersonalizationStateError|null;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  override connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosPhotosByAlbumIdElement['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosPhotosByAlbumIdElement['albumsShared_']>(
        'albumsShared_', state => state.wallpaper.googlePhotos.albumsShared);
    this.watch<GooglePhotosPhotosByAlbumIdElement['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<GooglePhotosPhotosByAlbumIdElement['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.watch<GooglePhotosPhotosByAlbumIdElement['photosByAlbumId_']>(
        'photosByAlbumId_',
        state => state.wallpaper.googlePhotos.photosByAlbumId);
    this.watch<GooglePhotosPhotosByAlbumIdElement['photosByAlbumIdLoading_']>(
        'photosByAlbumIdLoading_',
        state => state.wallpaper.loading.googlePhotos.photosByAlbumId);
    this.watch<
        GooglePhotosPhotosByAlbumIdElement['photosByAlbumIdResumeTokens_']>(
        'photosByAlbumIdResumeTokens_',
        state => state.wallpaper.googlePhotos.resumeTokens.photosByAlbumId);
    this.watch<GooglePhotosPhotosByAlbumIdElement['error_']>(
        'error_', state => state.error);

    this.updateFromStore();
  }

  /** Invoked on grid scroll threshold reached. */
  private onGridScrollThresholdReached_() {
    // Ignore this event if fired during initialization.
    if (!this.$.gridScrollThreshold.scrollHeight || !this.albumId) {
      this.$.gridScrollThreshold.clearTriggers();
      return;
    }

    // Ignore this event if photos are already being loaded.
    if (this.photosByAlbumIdLoading_ &&
        this.photosByAlbumIdLoading_[this.albumId] === true) {
      return;
    }


    // Ignore this event if there is no resume token (indicating there are no
    // additional photos to load).
    if (!this.photosByAlbumIdResumeTokens_ ||
        !this.photosByAlbumIdResumeTokens_[this.albumId]) {
      return;
    }

    // Fetch the next page of photos.
    fetchGooglePhotosAlbum(
        this.wallpaperProvider_, this.getStore(), this.albumId);
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_(hidden:
                               GooglePhotosPhotosByAlbumIdElement['hidden']) {
    if (hidden && this.error_ && this.error_.id === ERROR_ID) {
      // If |hidden|, the error associated with this element will have lost
      // user-facing context so it should be dismissed.
      this.dispatch(dismissErrorAction(ERROR_ID, /*fromUser=*/ false));
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));

    // When the user reselects an album that previously failed to load we should
    // automatically retry loading the selected album. Placeholders should be
    // shown while loading is in progress.
    if (this.albumId && this.photosByAlbumId_ && this.photosByAlbumIdLoading_ &&
        this.photosByAlbumId_[this.albumId] === null &&
        !this.photosByAlbumIdLoading_[this.albumId]) {
      fetchGooglePhotosAlbum(
          this.wallpaperProvider_, this.getStore(), this.albumId);
      this.album_ = getPlaceholders();
    }
  }

  /** Invoked on changes to |albumId|, |albums_|, or |photosByAlbumId_|. */
  private onAlbumIdOrAlbumsOrPhotosByAlbumIdChanged_(
      albumId: GooglePhotosPhotosByAlbumIdElement['albumId'],
      albums: GooglePhotosPhotosByAlbumIdElement['albums_'],
      albumsShared: GooglePhotosPhotosByAlbumIdElement['albumsShared_'],
      photosByAlbumId: GooglePhotosPhotosByAlbumIdElement['photosByAlbumId_']) {
    // If no album is currently selected there is nothing to display.
    if (!albumId) {
      this.album_ = [];
      return;
    }

    // If the album associated with |albumId| or |photosByAlbumId| have not yet
    // been set, there is nothing to display except placeholders. This occurs
    // if the user refreshes the wallpaper app while its navigated to a Google
    // Photos album.
    const matchingAlbum =
        findAlbumById(albumId, albums) ?? findAlbumById(albumId, albumsShared);
    if (!matchingAlbum || !photosByAlbumId) {
      this.album_ = getPlaceholders();
      return;
    }

    // If the currently selected album has not already been fetched, do so
    // though there is still nothing to display except placeholders.
    if (!photosByAlbumId.hasOwnProperty(albumId)) {
      fetchGooglePhotosAlbum(this.wallpaperProvider_, this.getStore(), albumId);
      this.album_ = getPlaceholders();
      return;
    }

    // If the currently selected album fails to load, display an error to the
    // user that allows them to make another attempt.
    if (photosByAlbumId[albumId] === null) {
      if (!this.hidden) {
        this.dispatch(setErrorAction({
          id: ERROR_ID,
          message: this.i18n('googlePhotosError'),
          dismiss: {
            message: this.i18n('googlePhotosTryAgain'),
            callback: (fromUser: boolean) => {
              if (fromUser) {
                // Post the reattempt instead of performing it immediately to
                // avoid updating the personalization store from the same
                // sequence that generated this event.
                setTimeout(
                    () => fetchGooglePhotosAlbum(
                        this.wallpaperProvider_, this.getStore(), albumId));
              }
            },
          },
        }));
      }
      return;
    }

    // NOTE: |album_| is updated in place to avoid resetting the scroll
    // position of the grid which would otherwise occur during reassignment.
    this.updateList(
        /*propertyPath=*/ 'album_',
        /*identityGetter=*/ (photo: GooglePhotosPhoto) => photo.id,
        /*newList=*/ photosByAlbumId[albumId] || [],
        /*identityBasedUpdate=*/ true);
  }

  /** Invoked on changes to |albumId| or |photosByAlbumIdResumeTokens_|. */
  private onAlbumIdOrPhotosByAlbumIdResumeTokensChanged_(
      albumId: GooglePhotosPhotosByAlbumIdElement['albumId'],
      photosByAlbumIdResumeTokens:
          GooglePhotosPhotosByAlbumIdElement['photosByAlbumIdResumeTokens_']) {
    if (albumId && photosByAlbumIdResumeTokens &&
        photosByAlbumIdResumeTokens[albumId]) {
      this.$.gridScrollThreshold.clearTriggers();
    }
  }

  /** Invoked on selection of a photo. `e.model.photo` is added by iron-list. */
  private onPhotoSelected_(e: WallpaperGridItemSelectedEvent&
                           {model: {photo: GooglePhotosPhoto}}) {
    assert(e.model.photo, 'google photos album photo selected event has photo');
    if (!this.isPhotoPlaceholder_(e.model.photo)) {
      selectWallpaper(e.model.photo, this.wallpaperProvider_, this.getStore());
      // Depends on whether shared albums feature is enabled, records Google
      // Photos source metric for all albums, owned albums or shared albums
      // accordingly.
      if (!this.isSharedAlbumsEnabled_) {
        recordWallpaperGooglePhotosSourceUMA(
            WallpaperGooglePhotosSource.ALBUMS);
      } else {
        const isAlbumShared =
            this.isAlbumShared_(this.albumId, this.albums_, this.albumsShared_);
        if (isAlbumShared !== null) {
          recordWallpaperGooglePhotosSourceUMA(
              isAlbumShared ? WallpaperGooglePhotosSource.SHARED_ALBUMS :
                              WallpaperGooglePhotosSource.OWNED_ALBUMS);
        }
      }
    }
  }

  /** Checks whether an album with albumId id is a shared or owned album. */
  private isAlbumShared_(
      albumId: GooglePhotosPhotosByAlbumIdElement['albumId'],
      albums: GooglePhotosPhotosByAlbumIdElement['albums_'],
      albumsShared: GooglePhotosPhotosByAlbumIdElement['albumsShared_']):
      boolean|null {
    if (findAlbumById(albumId, albums)) {
      return false;
    } else if (findAlbumById(albumId, albumsShared)) {
      return true;
    }
    console.warn('No matching album id found. ', albumId);
    return null;
  }


  /** Returns the aria label for the specified |photo|. */
  private getPhotoAriaLabel_(photo: GooglePhotosPhoto|null): string|undefined {
    if (photo) {
      return photo.id === PLACEHOLDER_ID ? this.i18n('ariaLabelLoading') :
                                           photo.name;
    }
    return undefined;
  }

  /** Returns the aria posinset index for the photo at index |i|. */
  private getPhotoAriaIndex_(i: number): number {
    return i + 1;
  }

  /** Returns whether the specified |photo| is a placeholder. */
  private isPhotoPlaceholder_(photo: GooglePhotosPhoto|null): boolean {
    return !!photo && photo.id === PLACEHOLDER_ID;
  }

  /** Returns whether the specified |photo| is currently selected. */
  private isPhotoSelected_(
      photo: GooglePhotosPhoto|null,
      currentSelected: GooglePhotosPhotosByAlbumIdElement['currentSelected_'],
      pendingSelected: GooglePhotosPhotosByAlbumIdElement['pendingSelected_']):
      boolean {
    if (!photo || (!currentSelected && !pendingSelected)) {
      return false;
    }
    // NOTE: Old clients may not support |dedupKey| when setting Google Photos
    // wallpaper, so use |id| in such cases for backwards compatibility.
    if (isGooglePhotosPhoto(pendingSelected) &&
        ((pendingSelected!.dedupKey &&
          isImageAMatchForKey(photo, pendingSelected!.dedupKey)) ||
         isImageAMatchForKey(photo, pendingSelected!.id))) {
      return true;
    }
    if (!pendingSelected && !!currentSelected &&
        (currentSelected.type === WallpaperType.kOnceGooglePhotos ||
         currentSelected.type === WallpaperType.kDailyGooglePhotos) &&
        isImageEqualToSelected(photo, currentSelected)) {
      return true;
    }
    return false;
  }
}

customElements.define(
    GooglePhotosPhotosByAlbumIdElement.is, GooglePhotosPhotosByAlbumIdElement);
