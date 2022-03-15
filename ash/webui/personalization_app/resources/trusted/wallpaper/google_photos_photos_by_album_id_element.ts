// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays Google Photos photos
 * for the currently selected album id.
 */

import './styles.js';
import '/common/styles.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSelectionEvent} from '../../common/utils.js';
import {CurrentWallpaper, GooglePhotosPhoto, WallpaperImage, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isGooglePhotosPhoto} from '../utils.js';

import {fetchGooglePhotosAlbum, selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

export interface GooglePhotosPhotosByAlbumId {
  $: {grid: IronListElement;};
}

export class GooglePhotosPhotosByAlbumId extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-photos-by-album-id';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      albumId: {
        type: String,
      },

      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      album_: {
        type: Array,
        computed:
            'computeAlbum_(albumId, photosByAlbumId_, photosByAlbumIdLoading_)',
      },

      currentSelected_: Object,
      pendingSelected_: Object,
      photosByAlbumId_: Object,
      photosByAlbumIdLoading_: Object,
    };
  }

  /** The currently selected album id. */
  albumId: string|undefined;

  /** Whether or not this element is currently hidden. */
  override hidden: boolean;

  /** The list of photos for the currently selected album id. */
  private album_: GooglePhotosPhoto[]|null|undefined;

  /** The currently selected wallpaper. */
  private currentSelected_: CurrentWallpaper|null;

  /** The pending selected wallpaper. */
  private pendingSelected_: FilePath|GooglePhotosPhoto|WallpaperImage|null;

  /** The list of photos by album id. */
  private photosByAlbumId_: Record<string, GooglePhotosPhoto[]|null|undefined>;

  /** Whether the list of photos by album id is currently loading. */
  private photosByAlbumIdLoading_: Record<string, boolean>;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  override connectedCallback() {
    super.connectedCallback();

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

    this.updateFromStore();
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_(hidden: GooglePhotosPhotosByAlbumId['hidden']) {
    if (hidden) {
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));
  }

  /** Invoked on selection of a photo. */
  private onPhotoSelected_(e: Event&{model: {photo: GooglePhotosPhoto}}) {
    assert(e.model.photo);
    if (isSelectionEvent(e)) {
      selectWallpaper(e.model.photo, this.wallpaperProvider_, this.getStore());
    }
  }

  /** Invoked to compute |album_|. */
  private computeAlbum_(
      albumId: GooglePhotosPhotosByAlbumId['albumId'],
      photosByAlbumId: GooglePhotosPhotosByAlbumId['photosByAlbumId_'],
      photosByAlbumIdLoading:
          GooglePhotosPhotosByAlbumId['photosByAlbumIdLoading_']):
      GooglePhotosPhoto[]|null {
    // If no album is currently selected or if the currently selected album is
    // still loading then there is nothing to display.
    if (!albumId || photosByAlbumIdLoading[albumId]) {
      return null;
    }

    // If the currently selected album has not already been fetched, do so
    // though there is still nothing to display.
    if (!photosByAlbumId.hasOwnProperty(albumId)) {
      fetchGooglePhotosAlbum(this.wallpaperProvider_, this.getStore(), albumId);
      return null;
    }

    // Once the currently selected album has been fetched it can be displayed.
    return photosByAlbumId[albumId]!;
  }

  // Returns whether the specified |photo| is currently selected.
  private isPhotoSelected_(
      photo: GooglePhotosPhoto|null,
      currentSelected: GooglePhotosPhotosByAlbumId['currentSelected_'],
      pendingSelected: GooglePhotosPhotosByAlbumId['pendingSelected_']):
      boolean {
    if (!photo || (!currentSelected && !pendingSelected)) {
      return false;
    }
    if (isGooglePhotosPhoto(pendingSelected) &&
        pendingSelected!.id === photo.id) {
      return true;
    }
    if (!pendingSelected &&
        currentSelected?.type === WallpaperType.kGooglePhotos &&
        currentSelected!.key === photo.id) {
      return true;
    }
    return false;
  }
}

customElements.define(
    GooglePhotosPhotosByAlbumId.is, GooglePhotosPhotosByAlbumId);
