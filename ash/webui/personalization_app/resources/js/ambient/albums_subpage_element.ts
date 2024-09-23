// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ambient albums subpage is to select personal albums in
 * Google Photos or categories in Art gallery.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './album_list_element.js';
import './art_album_dialog_element.js';

import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';

import {AmbientModeAlbum, TopicSource} from '../../personalization_app.mojom-webui.js';
import {logAmbientModeLinkToGooglePhotosClick} from '../personalization_metrics_logger.js';
import {PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getNumberOfGridItemsPerRow} from '../utils.js';

import {AlbumSelectedChangedEvent} from './album_list_element.js';
import {getTemplate} from './albums_subpage_element.html.js';
import {setAlbumSelected} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientObserver} from './ambient_observer.js';

/** Height in pixels of a tile. */
const kTileHeightPx = 136;

export class AlbumsSubpageElement extends WithPersonalizationStore {
  static get is() {
    return 'albums-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      topicSource: TopicSource,
      albums: {
        type: Array,
        // Set to null to differentiate from an empty album.
        value: null,
      },
      ambientModeEnabled_: {
        type: Boolean,
        observer: 'onAmbientModeEnabledChanged_',
      },
      showArtAlbumDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  topicSource: TopicSource;
  albums: AmbientModeAlbum[]|null = null;
  loadingAlbums: boolean;

  private ambientModeEnabled_: boolean|null;
  private showArtAlbumDialog_: boolean;

  override ready() {
    super.ready();
    this.addEventListener(
        'album_selected_changed', this.onAlbumSelectedChanged_.bind(this));
  }

  override connectedCallback() {
    super.connectedCallback();
    AmbientObserver.initAmbientObserverIfNeeded();
    this.watch<AlbumsSubpageElement['ambientModeEnabled_']>(
        'ambientModeEnabled_', state => state.ambient.ambientModeEnabled);
    this.updateFromStore();
    getAmbientProvider().fetchSettingsAndAlbums();
  }

  private shouldShowContent_(): boolean {
    return this.ambientModeEnabled_ !== null && this.ambientModeEnabled_;
  }

  private getTitleInnerHtml_(): string|TrustedHTML {
    switch (this.topicSource) {
      case TopicSource.kGooglePhotos:
        return this.i18nAdvanced('ambientModeAlbumsSubpageGooglePhotosTitle');
      case TopicSource.kArtGallery:
        return this.i18n('ambientModeTopicSourceArtGalleryDescription');
      case TopicSource.kVideo:
        return this.i18n('ambientModeTopicSourceVideoDescription');
    }
  }

  /**
   * List of loading tiles to be displayed to the user when albums are loading.
   */
  private getLoadingTiles_(): number[] {
    const x = getNumberOfGridItemsPerRow();
    const y = Math.floor(this.offsetHeight / kTileHeightPx);
    return new Array(x * y).fill(0);
  }

  private loadingAlbums_(): boolean {
    return this.albums === null || this.topicSource === null;
  }

  private showNoGoogleAlbums_(): boolean {
    if (this.topicSource !== TopicSource.kGooglePhotos) {
      return false;
    }
    return !isNonEmptyArray(this.albums);
  }

  private onAlbumSelectedChanged_(event: AlbumSelectedChangedEvent) {
    const albumChanged = event.detail.album;
    if (this.topicSource === TopicSource.kArtGallery) {
      const anySelected = this.albums!.some(album => album.checked);
      // For art gallery, cannot deselect all the albums. Show a dialog to users
      // and select the album automatically.
      if (!anySelected) {
        this.showArtAlbumDialog_ = true;
        const albumIndex =
            this.albums!.findIndex(album => album.id === albumChanged.id);
        assert(albumIndex >= 0);
        this.set(`albums.${albumIndex}.checked`, true);
        return;
      }
    }
    setAlbumSelected(albumChanged, getAmbientProvider(), this.getStore());
  }

  private onArtAlbumDialogClose_() {
    this.showArtAlbumDialog_ = false;
  }

  private onAmbientModeEnabledChanged_(ambientModeEnabled: boolean|null) {
    if (ambientModeEnabled !== null && !ambientModeEnabled) {
      PersonalizationRouterElement.reloadAtAmbient();
    }
  }

  private onGooglePhotosLinkClicked_(event: Event) {
    event.stopPropagation();
    logAmbientModeLinkToGooglePhotosClick();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'albums-subpage': AlbumsSubpageElement;
  }
}

customElements.define(AlbumsSubpageElement.is, AlbumsSubpageElement);
