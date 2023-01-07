// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects.
 */

import '../../css/wallpaper.css.js';

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WallpaperCollection} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isNonEmptyArray} from '../utils.js';

import {CollectionsGrid} from './collections_grid_element.js';
import {DefaultImageSymbol} from './constants.js';
import {getTemplate} from './wallpaper_collections_element.html.js';
import {initializeBackdropData} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

function collectionsError(
    collections: WallpaperCollection[]|null,
    collectionsLoading: boolean): boolean {
  return !collectionsLoading && !isNonEmptyArray(collections);
}

function localImagesError(
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean): boolean {
  return !localImagesLoading && !isNonEmptyArray(localImages);
}

function hasError(
    collections: WallpaperCollection[]|null, collectionsLoading: boolean,
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean): boolean {
  return localImagesError(localImages, localImagesLoading) &&
      collectionsError(collections, collectionsLoading);
}


export interface WallpaperCollections {
  $: {collectionsGrid: CollectionsGrid};
}

export class WallpaperCollections extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-collections';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Hidden state of this element. Used to notify of visibility changes. */
      hidden: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      hasError_: Boolean,
    };
  }

  override hidden: boolean;
  private hasError_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.watch<boolean>(
        'hasError_',
        state => hasError(
            state.wallpaper.backdrop.collections,
            state.wallpaper.loading.collections, state.wallpaper.local.images,
            state.wallpaper.loading.local.images));
    this.updateFromStore();
    initializeBackdropData(getWallpaperProvider(), this.getStore());
  }

  /**
   * Notify that this element visibility has changed.
   */
  private async onHiddenChanged_(hidden: boolean) {
    if (!hidden) {
      document.title = this.i18n('wallpaperLabel');
    }
    afterNextRender(this, () => this.notifyResize());
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);
