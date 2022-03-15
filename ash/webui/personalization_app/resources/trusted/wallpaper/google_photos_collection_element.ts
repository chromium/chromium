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
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isNonEmptyArray} from '../../common/utils.js';
import {GooglePhotosAlbum, GooglePhotosPhoto, WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
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
  override hidden: boolean;

  /** The list of albums. */
  private albums_: GooglePhotosAlbum[]|null|undefined;

  /** The list of photos. */
  private photos_: GooglePhotosPhoto[]|null|undefined;

  /** The currently selected tab. */
  private tab_: Tab;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  override connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosCollection['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosCollection['photos_']>(
        'photos_', state => state.wallpaper.googlePhotos.photos);

    this.updateFromStore();

    initializeGooglePhotosData(this.wallpaperProvider_, this.getStore());
  }

  /** Invoked on changes to the currently selected |albumId|. */
  private onAlbumIdChanged_(albumId: GooglePhotosCollection['albumId']) {
    this.tab_ = albumId ? Tab.PhotosByAlbumId : Tab.Albums;
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_(hidden: GooglePhotosCollection['hidden']) {
    if (hidden) {
      return;
    }

    document.title = this.i18n('googlePhotosLabel');
    this.$.main.focus();
  }

  /** Invoked on tab selected. */
  private onTabSelected_(e: Event) {
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
  private isAlbumsEmpty_(albums: GooglePhotosCollection['albums_']): boolean {
    return !isNonEmptyArray(albums);
  }

  /** Whether the albums tab is currently selected. */
  private isAlbumsTabSelected_(tab: GooglePhotosCollection['tab_']): boolean {
    return tab === Tab.Albums;
  }

  /** Whether the albums tab is currently visible. */
  private isAlbumsTabVisible_(
      hidden: GooglePhotosCollection['hidden'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return this.isAlbumsTabSelected_(tab) && !hidden;
  }

  /** Whether the photos by album id tab is currently visible. */
  private isPhotosByAlbumIdTabVisible_(
      hidden: GooglePhotosCollection['hidden'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return tab === Tab.PhotosByAlbumId && !hidden;
  }

  /** Whether the list of photos is empty. */
  private isPhotosEmpty_(photos: GooglePhotosCollection['photos_']): boolean {
    return !isNonEmptyArray(photos);
  }

  /** Whether the photos tab is currently selected. */
  private isPhotosTabSelected_(tab: GooglePhotosCollection['tab_']): boolean {
    return tab === Tab.Photos;
  }

  /** Whether the photos tab is currently visible. */
  private isPhotosTabVisible_(
      hidden: GooglePhotosCollection['hidden'],
      tab: GooglePhotosCollection['tab_']): boolean {
    return this.isPhotosTabSelected_(tab) && !hidden;
  }

  /** Whether the tab strip is currently visible. */
  private isTabStripVisible_(
      albumId: GooglePhotosCollection['albumId'],
      albums: GooglePhotosCollection['albums_']): boolean {
    return !albumId && !this.isAlbumsEmpty_(albums);
  }
}

customElements.define(GooglePhotosCollection.is, GooglePhotosCollection);
