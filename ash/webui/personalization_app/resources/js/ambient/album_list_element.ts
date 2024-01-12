// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying a list of albums.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {assert} from 'chrome://resources/js/assert.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {AmbientModeAlbum, TopicSource} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getCountText, isRecentHighlightsAlbum} from '../utils.js';

import {getTemplate} from './album_list_element.html.js';

export interface AlbumListElement {
  $: {grid: IronListElement};
}

export type AlbumSelectedChangedEvent = CustomEvent<{album: AmbientModeAlbum}>;

declare global {
  interface HTMLElementEventMap {
    'album_selected_changed': AlbumSelectedChangedEvent;
  }
}

export class AlbumListElement extends WithPersonalizationStore {
  static get is() {
    return 'album-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      topicSource: TopicSource,
      /**
       * List of albums received from the client.
       */
      albums: {
        type: Array,
        value: null,
        observer: 'onAlbumsChanged_',
      },
      /**
       * List of albums used for iron-list rendering.
       */
      albumsForDisplay_: {
        type: Array,
        value: [],
      },
    };
  }

  topicSource: TopicSource;
  albums: AmbientModeAlbum[]|null;
  private albumsForDisplay_: AmbientModeAlbum[];

  private onAlbumsChanged_(albums: AlbumListElement['albums']) {
    if (!albums) {
      return;
    }

    // `albumsForDisplay_` is updated in place to avoid complete re-rendering of
    // iron-list, which would cause the tabindex to reset. See b/291123326.
    this.updateList(
        /*propertyPath=*/ 'albumsForDisplay_',
        /*identityGetter=*/
        (album: AmbientModeAlbum) => album.id,
        /*newList=*/ albums,
        /*identityBasedUpdate=*/ true,
    );
  }

  /** Invoked on selection of an album. */
  private onAlbumSelected_(e: Event&{model: {album: AmbientModeAlbum}}) {
    // Retrieve the actual instance of selected album from `albums`.
    const albumIndex =
        this.albums!.findIndex(album => album.id === e.model.album.id);
    assert(albumIndex >= 0);
    const albumChanged = this.albums![albumIndex];

    if (this.topicSource === TopicSource.kVideo && albumChanged.checked) {
      // De-selecting a selected video album is a no-op. Selecting a different
      // video album will unselect the other video albums in the client.
      return;
    }

    albumChanged.checked = !albumChanged.checked;
    this.dispatchEvent(new CustomEvent(
        'album_selected_changed',
        {bubbles: true, composed: true, detail: {album: albumChanged}}));
  }

  private isAlbumSelected_(
      changedAlbum: AmbientModeAlbum|null,
      albums: AmbientModeAlbum[]|null): boolean {
    if (!changedAlbum) {
      return false;
    }
    const album = albums!.find(album => album.id === changedAlbum.id);
    return !!album && album.checked;
  }

  private getAlbumItemClass_(
      album: AmbientModeAlbum|null, albums: AmbientModeAlbum[]|null): string {
    return album && this.isAlbumSelected_(album, albums) ?
        'album album-selected' :
        'album';
  }

  /** Returns the secondary text to display for the specified |album|. */
  private getSecondaryText_(
      album: AmbientModeAlbum|null, topicSource: TopicSource): string {
    if (!album) {
      return '';
    }
    if (topicSource === TopicSource.kGooglePhotos) {
      if (isRecentHighlightsAlbum(album)) {
        return this.i18n('ambientModeAlbumsSubpageRecentHighlightsDesc');
      }
      return getCountText(album.numberOfPhotos);
    }
    if (this.topicSource === TopicSource.kArtGallery ||
        this.topicSource === TopicSource.kVideo) {
      return album.description;
    }
    return '';
  }

  private getAriaIndex_(index: number): number {
    return index + 1;
  }

  private isGooglePhotos_(topicSource: TopicSource): boolean {
    return topicSource === TopicSource.kGooglePhotos;
  }

  private isVideo_(topicSource: TopicSource): boolean {
    return topicSource === TopicSource.kVideo;
  }
}

customElements.define(AlbumListElement.is, AlbumListElement);
