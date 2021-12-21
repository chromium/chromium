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

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isNonEmptyArray} from '../../common/utils.js';
import {WallpaperCollection, WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {initializeGooglePhotosData} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

/** Enumeration of supported tabs. */
enum Tab {
  Albums,
  Photos,
  PhotosByAlbumId,
}

export interface GooglePhotosCollection {
  $: {main: HTMLElement;};
}

export class GooglePhotosCollection extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-collection';
  }

  static get template() {
    return html`{__html_template__}`;
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

      albums_: Array,
      photos_: Array,

      tab_: {
        type: String,
        value: Tab.Photos,
      },
    };
  }

  /** The currently selected album id. */
  albumId: string|undefined;

  /** Whether or not this element is currently hidden. */
  hidden: boolean;

  /** The list of albums. */
  private albums_: WallpaperCollection[]|null|undefined;

  /** The list of photos. */
  private photos_: Url[]|null|undefined;

  /** The currently selected tab. */
  private tab_: Tab;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosCollection['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosCollection['photos_']>(
        'photos_', state => state.wallpaper.googlePhotos.photos);

    this.updateFromStore();

    initializeGooglePhotosData(this.wallpaperProvider_, this.getStore());
  }

  /** Invoked on changes to the currently selected |albumId|. */
  private onAlbumIdChanged_() {
    this.tab_ = this.albumId ? Tab.PhotosByAlbumId : Tab.Albums;
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_() {
    if (this.hidden) {
      return;
    }

    document.title = this.i18n('googlePhotosLabel');
    this.$.main.focus();
  }

  /** Invoked on tab selected. */
  onTabSelected_(e: Event) {
    const currentTarget: HTMLElement = e.currentTarget as HTMLElement;
    switch (currentTarget.id) {
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

  /** Whether the list of albums is empty. */
  private isAlbumsEmpty_(): boolean {
    return !isNonEmptyArray(this.albums_);
  }

  /** Whether the albums tab is currently selected. */
  private isAlbumsTabSelected_(): boolean {
    return this.tab_ === Tab.Albums;
  }

  /** Whether the albums tab is currently visible. */
  private isAlbumsTabVisible_(): boolean {
    return this.isAlbumsTabSelected_() && !this.hidden;
  }

  /** Whether the photos by album id tab is currently visible. */
  private isPhotosByAlbumIdTabVisible_(): boolean {
    return this.tab_ === Tab.PhotosByAlbumId && !this.hidden;
  }

  /** Whether the list of photos is empty. */
  private isPhotosEmpty_(): boolean {
    return !isNonEmptyArray(this.photos_);
  }

  /** Whether the photos tab is currently selected. */
  private isPhotosTabSelected_(): boolean {
    return this.tab_ === Tab.Photos;
  }

  /** Whether the photos tab is currently visible. */
  private isPhotosTabVisible_(): boolean {
    return this.isPhotosTabSelected_() && !this.hidden;
  }

  /** Whether the tab strip is currently visible. */
  private isTabStripVisible_(): boolean {
    return !this.albumId && !this.isAlbumsEmpty_();
  }
}

customElements.define(GooglePhotosCollection.is, GooglePhotosCollection);
