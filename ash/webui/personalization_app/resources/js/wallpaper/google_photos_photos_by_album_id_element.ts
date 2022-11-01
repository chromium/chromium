// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays Google Photos photos
 * for the currently selected album id.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import '../../css/wallpaper.css.js';
import '../../css/common.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {dismissErrorAction, setErrorAction} from '../personalization_actions.js';
import {CurrentWallpaper, GooglePhotosAlbum, GooglePhotosPhoto, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {PersonalizationStateError} from '../personalization_state.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {DisplayableImage} from './constants.js';
import {recordWallpaperGooglePhotosSourceUMA, WallpaperGooglePhotosSource} from './google_photos_metrics_logger.js';
import {getTemplate} from './google_photos_photos_by_album_id_element.html.js';
import {getLoadingPlaceholders, isGooglePhotosPhoto, isImageAMatchForKey, isImageEqualToSelected} from './utils.js';
import {fetchGooglePhotosAlbum, selectWallpaper} from './wallpaper_controller.js';
import {WallpaperGridItemSelectedEvent} from './wallpaper_grid_item_element.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const ERROR_ID = 'GooglePhotosByAlbumId';
const PLACEHOLDER_ID = 'placeholder';

/** Returns placeholders to show while Google Photos photos are loading. */
function getPlaceholders(): GooglePhotosPhoto[] {
  return getLoadingPlaceholders(() => {
    const photo = new GooglePhotosPhoto();
    photo.id = PLACEHOLDER_ID;
    return photo;
  });
}

export interface GooglePhotosPhotosByAlbumId {
  $: {grid: IronListElement, gridScrollThreshold: IronScrollThresholdElement};
}

export class GooglePhotosPhotosByAlbumId extends WithPersonalizationStore {
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

      albums: Array,
      currentSelected_: Object,
      pendingSelected_: Object,
      photosByAlbumId_: Object,
      photosByAlbumIdLoading_: Object,
      photosByAlbumIdResumeTokens_: Object,

      error_: {
        type: Object,
        value: null,
      },
    };
  }

  static get observers() {
    return [
      'onAlbumIdOrAlbumsOrPhotosByAlbumIdChanged_(albumId, albums_, photosByAlbumId_)',
      'onAlbumIdOrPhotosByAlbumIdResumeTokensChanged_(albumId, photosByAlbumIdResumeTokens_)',
    ];
  }

  /** The currently selected album id. */
  albumId: string|undefined;

  /** Whether or not this element is currently hidden. */
  override hidden: boolean;

  /** The list of photos for the currently selected album id. */
  private album_: GooglePhotosPhoto[];

  /** The list of albums. */
  private albums_: GooglePhotosAlbum[]|null|undefined;

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

  /** The current personalization error state. */
  private error_: PersonalizationStateError|null;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  override connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosPhotosByAlbumId['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosPhotosByAlbumId['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<GooglePhotosPhotosByAlbumId['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.watch<GooglePhotosPhotosByAlbumId['photosByAlbumId_']>(
        'photosByAlbumId_',
        state => state.wallpaper.googlePhotos.photosByAlbumId);
    this.watch<GooglePhotosPhotosByAlbumId['photosByAlbumIdLoading_']>(
        'photosByAlbumIdLoading_',
        state => state.wallpaper.loading.googlePhotos.photosByAlbumId);
    this.watch<GooglePhotosPhotosByAlbumId['photosByAlbumIdResumeTokens_']>(
        'photosByAlbumIdResumeTokens_',
        state => state.wallpaper.googlePhotos.resumeTokens.photosByAlbumId);
    this.watch<GooglePhotosPhotosByAlbumId['error_']>(
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
  private onHiddenChanged_(hidden: GooglePhotosPhotosByAlbumId['hidden']) {
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
      albumId: GooglePhotosPhotosByAlbumId['albumId'],
      albums: GooglePhotosPhotosByAlbumId['albums_'],
      photosByAlbumId: GooglePhotosPhotosByAlbumId['photosByAlbumId_']) {
    // If no album is currently selected there is nothing to display.
    if (!albumId) {
      this.album_ = [];
      return;
    }

    // If the album associated with |albumId| or |photosByAlbumId| have not yet
    // been set, there is nothing to display except placeholders. This occurs
    // if the user refreshes the wallpaper app while its navigated to a Google
    // Photos album.
    if (!Array.isArray(albums) || !albums.some(album => album.id === albumId) ||
        !photosByAlbumId) {
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
      albumId: GooglePhotosPhotosByAlbumId['albumId'],
      photosByAlbumIdResumeTokens:
          GooglePhotosPhotosByAlbumId['photosByAlbumIdResumeTokens_']) {
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
      recordWallpaperGooglePhotosSourceUMA(WallpaperGooglePhotosSource.ALBUMS);
    }
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
      currentSelected: GooglePhotosPhotosByAlbumId['currentSelected_'],
      pendingSelected: GooglePhotosPhotosByAlbumId['pendingSelected_']):
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
    GooglePhotosPhotosByAlbumId.is, GooglePhotosPhotosByAlbumId);
