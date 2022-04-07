// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays Google Photos albums.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import './styles.js';
import '../../common/styles.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getCountText, getLoadingPlaceholders, isSelectionEvent} from '../../common/utils.js';
import {GooglePhotosAlbum, WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
import {PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getTemplate} from './google_photos_albums_element.html.js';

import {fetchGooglePhotosAlbums} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const PLACEHOLDER_ID = 'placeholder';

/** Returns placeholders to show while Google Photos albums are loading. */
function getPlaceholders(): GooglePhotosAlbum[] {
  return getLoadingPlaceholders(() => {
    const album = new GooglePhotosAlbum();
    album.id = PLACEHOLDER_ID;
    return album;
  });
}

export interface GooglePhotosAlbums {
  $: {grid: IronListElement, gridScrollThreshold: IronScrollThresholdElement};
}

export class GooglePhotosAlbums extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-albums';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      albums_: {
        type: Array,
        observer: 'onAlbumsChanged_',
      },

      albumsForDisplay_: {
        type: Array,
        value: getPlaceholders,
      },

      albumsLoading_: Boolean,

      albumsResumeToken_: {
        type: String,
        observer: 'onAlbumsResumeTokenChanged_',
      },
    };
  }

  /** Whether or not this element is currently hidden. */
  override hidden: boolean;

  /** The list of albums. */
  private albums_: GooglePhotosAlbum[]|null|undefined;

  /** The list of |albums_| which is updated in place for display. */
  private albumsForDisplay_: GooglePhotosAlbum[];

  /** Whether the list of albums is currently loading. */
  private albumsLoading_: boolean;

  /** The resume token needed to fetch the next page of albums. */
  private albumsResumeToken_: string|null;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  override connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosAlbums['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosAlbums['albumsLoading_']>(
        'albumsLoading_', state => state.wallpaper.loading.googlePhotos.albums);
    this.watch<GooglePhotosAlbums['albumsResumeToken_']>(
        'albumsResumeToken_',
        state => state.wallpaper.googlePhotos.resumeTokens.albums);

    this.updateFromStore();
  }

  /** Invoked on selection of an album. */
  private onAlbumSelected_(e: Event&{model: {album: GooglePhotosAlbum}}) {
    assert(e.model.album);
    if (!this.isAlbumPlaceholder_(e.model.album) && isSelectionEvent(e)) {
      PersonalizationRouter.instance().selectGooglePhotosAlbum(e.model.album);
    }
  }

  /** Invoked on changes to |albums_|. */
  private onAlbumsChanged_(albums: GooglePhotosAlbums['albums_']) {
    // NOTE: |albumsForDisplay_| is updated in place to avoid resetting the
    // scroll position of the grid which would otherwise occur during
    // reassignment but it will be deeply equal to |albums_| after updating.
    this.updateList(
        /*propertyPath=*/ 'albumsForDisplay_',
        /*identityGetter=*/ (album: GooglePhotosAlbum) => album.id,
        /*newList=*/ albums || [],
        /*identityBasedUpdate=*/ true);
  }

  /** Invoked on changes to |albumsResumeToken_|. */
  private onAlbumsResumeTokenChanged_(
      albumsResumeToken: GooglePhotosAlbums['albumsResumeToken_']) {
    if (albumsResumeToken) {
      this.$.gridScrollThreshold.clearTriggers();
    }
  }

  /** Invoked on grid scroll threshold reached. */
  private onGridScrollThresholdReached_() {
    // Ignore this event if fired during initialization.
    if (!this.$.gridScrollThreshold.scrollHeight) {
      this.$.gridScrollThreshold.clearTriggers();
      return;
    }

    // Ignore this event if albums are already being loading or if there is no
    // resume token (indicating there are no additional albums to load).
    if (this.albumsLoading_ === true || !this.albumsResumeToken_) {
      return;
    }

    // Fetch the next page of albums.
    fetchGooglePhotosAlbums(this.wallpaperProvider_, this.getStore());
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_(hidden: GooglePhotosAlbums['hidden']) {
    if (hidden) {
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));
  }

  /** Returns the secondary text to display for the specified |album|. */
  private getSecondaryText_(album: GooglePhotosAlbum): string {
    return getCountText(album.photoCount);
  }

  /** Returns whether the specified |album| is a placeholder. */
  private isAlbumPlaceholder_(album: GooglePhotosAlbum): boolean {
    return album.id === PLACEHOLDER_ID;
  }
}

customElements.define(GooglePhotosAlbums.is, GooglePhotosAlbums);
