// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays the Google Photos
 * collection.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './styles.js';
import '/common/styles.js';

import {isNonEmptyArray} from '/common/utils.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {initializeGooglePhotosData} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

/**
 * Enumeration of supported tabs.
 * @enum {string}
 */
const Tab = {
  Albums: 'albums',
  Photos: 'photos',
  PhotosByAlbumId: 'photosByAlbumId',
};

/** @polymer */
export class GooglePhotosCollection extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-collection';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The currently selected album id.
       * @type {?string}
       */
      albumId: {
        type: String,
        observer: 'onAlbumIdChanged_',
      },

      /**
       * Whether or not this element is currently hidden.
       * @type {boolean}
       */
      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      /**
       * The list of albums.
       * @type {?Array<WallpaperCollection>}
       * @private
       */
      albums_: {
        type: Array,
      },

      /**
       * The list of photos.
       * @type {?Array<undefined>}
       * @private
       */
      photos_: {
        type: Array,
      },

      /**
       * The currently selected tab.
       * @type {!Tab}
       * @private
       */
      tab_: {
        type: String,
        value: Tab.Photos,
      },
    };
  }

  /** @override */
  constructor() {
    super();
    /** @const @private */
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.watch('albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch('photos_', state => state.wallpaper.googlePhotos.photos);
    this.updateFromStore();

    initializeGooglePhotosData(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Invoked on changes to the currently selected |albumId|.
   * @private
   */
  onAlbumIdChanged_() {
    this.tab_ = this.albumId ? Tab.PhotosByAlbumId : Tab.Albums;
  }

  /**
   * Invoked on changes to this element's |hidden| state.
   * @private
   */
  onHiddenChanged_() {
    if (this.hidden) {
      return;
    }

    document.title = this.i18n('googlePhotosLabel');
    this.shadowRoot.getElementById('main').focus();
  }

  /**
   * Invoked on tab selected.
   * @param {!Event} e
   * @private
   */
  onTabSelected_(e) {
    switch (e.currentTarget.id) {
      case 'albumsTab':
        this.tab_ = Tab.Albums;
        return;
      case 'photosTab':
        this.tab_ = Tab.Photos;
        return;
      default:
        assertNotReached();
        return;
    }
  }

  /**
   * Whether the list of albums is empty.
   * @return {boolean}
   * @private
   */
  isAlbumsEmpty_() {
    return !isNonEmptyArray(this.albums_);
  }

  /**
   * Whether the albums tab is currently selected.
   * @return {boolean}
   * @private
   */
  isAlbumsTabSelected_() {
    return this.tab_ === Tab.Albums;
  }

  /**
   * Whether the albums tab is currently visible.
   * @return {boolean}
   * @private
   */
  isAlbumsTabVisible_() {
    return this.isAlbumsTabSelected_() && !this.hidden;
  }

  /**
   * Whether the photos by album id tab is currently visible.
   * @return {boolean}
   * @private
   */
  isPhotosByAlbumIdTabVisible_() {
    return this.tab_ === Tab.PhotosByAlbumId && !this.hidden;
  }

  /**
   * Whether the list of photos is empty.
   * @return {boolean}
   * @private
   */
  isPhotosEmpty_() {
    return !isNonEmptyArray(this.photos_);
  }

  /**
   * Whether the photos tab is currently selected.
   * @return {boolean}
   * @private
   */
  isPhotosTabSelected_() {
    return this.tab_ === Tab.Photos;
  }

  /**
   * Whether the photos tab is currently visible.
   * @return {boolean}
   * @private
   */
  isPhotosTabVisible_() {
    return this.isPhotosTabSelected_() && !this.hidden;
  }

  /**
   * Whether the tab strip is currently visible.
   * @return {boolean}
   * @private
   */
  isTabStripVisible_() {
    return !this.albumId && !this.isAlbumsEmpty_();
  }
}

customElements.define(GooglePhotosCollection.is, GooglePhotosCollection);
