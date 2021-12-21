// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays Google Photos photos
 * for the currently selected album id.
 */

import './styles.js';
import '/common/styles.js';

import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isNonEmptyArray} from '../../common/utils.js';
import {WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore, WithPersonalizationStore} from '../personalization_store.js';

import {fetchGooglePhotosAlbum} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

let fetchGooglePhotosAlbumFunction = fetchGooglePhotosAlbum;

type WallpaperControllerFunctionPromisesForTesting = {
  fetchGooglePhotosAlbum: Promise<any[]>,
};

/**
 * TODO(dmblack): Remove once mojo provider is used to fetch data.
 * Mocks out wallpaper controller functions for testing. Returns promises that
 * are resolved when the function is called by |GooglePhotosPhotosByAlbumId|.
 */
export function promisifyWallpaperControllerFunctionsForTesting():
    WallpaperControllerFunctionPromisesForTesting {
  const resolvers: Record<string, (args: any[]) => void> = {};
  const promises = {
    fetchGooglePhotosAlbum: new Promise<any[]>(
        resolve => resolvers[fetchGooglePhotosAlbum.name] = resolve),
  };
  fetchGooglePhotosAlbumFunction = (...args: any): Promise<void> => {
    resolvers[fetchGooglePhotosAlbum.name](args);
    return Promise.resolve();
  };
  return promises;
}

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

      photosByAlbumId_: Object,
      photosByAlbumIdLoading_: Object,
    };
  }

  /** The currently selected album id. */
  albumId: string|undefined;

  /** Whether or not this element is currently hidden. */
  hidden: boolean;

  /** The list of photos for the currently selected album id. */
  private album_: unknown[]|null|undefined;

  /** The list of photos by album id. */
  private photosByAlbumId_: Record<string, unknown[]|null|undefined>;

  /** Whether the list of photos by album id is currently loading. */
  private photosByAlbumIdLoading_: Record<string, boolean>;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosPhotosByAlbumId['photosByAlbumId_']>(
        'photosByAlbumId_',
        state => state.wallpaper.googlePhotos.photosByAlbumId);
    this.watch<GooglePhotosPhotosByAlbumId['photosByAlbumIdLoading_']>(
        'photosByAlbumIdLoading_',
        state => state.wallpaper.loading.googlePhotos.photosByAlbumId);

    this.updateFromStore();
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_() {
    if (this.hidden) {
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));
  }

  /** Invoked to compute |album_|. */
  private computeAlbum_(): number[]|null|undefined {
    // If no album is currently selected or if the currently selected album is
    // still loading then there is nothing to display.
    if (!this.albumId || this.photosByAlbumIdLoading_[this.albumId]) {
      return null;
    }

    // If the currently selected album has not already been fetched, do so
    // though there is still nothing to display.
    if (!this.photosByAlbumId_.hasOwnProperty(this.albumId)) {
      fetchGooglePhotosAlbumFunction(
          this.wallpaperProvider_, this.getStore(), this.albumId);
      return null;
    }

    // Once the currently selected album has been fetched it can be displayed.
    return this.photosByAlbumId_[this.albumId]?.map((_, i) => i + 1);
  }
}

customElements.define(
    GooglePhotosPhotosByAlbumId.is, GooglePhotosPhotosByAlbumId);
