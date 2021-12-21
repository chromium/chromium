// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays Google Photos albums.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './styles.js';
import '../../common/styles.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSelectionEvent} from '../../common/utils.js';
import {WallpaperCollection} from '../personalization_app.mojom-webui.js';
import {PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

export interface GooglePhotosAlbums {
  $: {grid: IronListElement;};
}

export class GooglePhotosAlbums extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-albums';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      albums_: Array,
      albumsLoading_: Boolean,
    };
  }

  /** Whether or not this element is currently hidden. */
  hidden: boolean;

  /** The list of albums. */
  private albums_: WallpaperCollection[]|null|undefined;

  /** Whether the list of albums is currently loading. */
  private albumsLoading_: boolean;

  connectedCallback() {
    super.connectedCallback();

    this.watch<GooglePhotosAlbums['albums_']>(
        'albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch<GooglePhotosAlbums['albumsLoading_']>(
        'albumsLoading_', state => state.wallpaper.loading.googlePhotos.albums);

    this.updateFromStore();
  }

  /** Invoked on selection of an album. */
  private onAlbumSelected_(e: Event&{model: {album: WallpaperCollection}}) {
    assert(e.model.album);
    if (isSelectionEvent(e)) {
      PersonalizationRouter.instance().selectGooglePhotosAlbum(e.model.album);
    }
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
}

customElements.define(GooglePhotosAlbums.is, GooglePhotosAlbums);
