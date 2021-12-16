// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The breadcrumb that displays the current view stack and allows users to
 * navigate.
 */

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../common/styles.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isNonEmptyArray} from '../common/utils.js';

import {WallpaperCollection} from './personalization_app.mojom-webui.js';
import {Paths} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {isNonEmptyString} from './utils.js';

/** Event interface for dom-repeat. */
interface RepeaterEvent extends CustomEvent {
  model: {
    index: number,
  };
}

export class PersonalizationBreadcrumb extends WithPersonalizationStore {
  static get is() {
    return 'personalization-breadcrumb';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The current collection id to display.
       */
      collectionId: {
        type: String,
      },

      /**
       * The current Google Photos album id to display.
       */
      googlePhotosAlbumId: {
        type: String,
      },

      /**
       * The current path of the page.
       */
      path: {
        type: String,
      },

      breadcrumbs_: {
        type: Array,
        computed:
            'computeBreadcrumbs_(path, collections_, collectionId, googlePhotosAlbums_, googlePhotosAlbumId)',
      },

      collections_: {
        type: Array,
      },

      /**
       * The list of Google Photos albums.
       */
      googlePhotosAlbums_: {
        type: Array,
      },

      showBackButton_: {
        type: Boolean,
        computed: 'computeShowBackButton_(path)',
      },
    };
  }

  collectionId: string;
  googlePhotosAlbumId: string;
  path: string;
  private breadcrumbs_: string[];
  private collections_: WallpaperCollection[]|null;
  private googlePhotosAlbums_: WallpaperCollection[]|null;
  private showBackButton_: boolean;

  connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.wallpaper.backdrop.collections);
    this.watch(
        'googlePhotosAlbums_', state => state.wallpaper.googlePhotos.albums);
    this.updateFromStore();
  }

  private computeBreadcrumbs_(
      path: string, collections: WallpaperCollection[]|null,
      collectionId: string, googlePhotosAlbums: WallpaperCollection[]|null,
      googlePhotosAlbumId: string|null): string[] {
    const breadcrumbs = [this.i18n('title')];

    switch (path) {
      case Paths.CollectionImages:
        if (isNonEmptyArray(collections)) {
          const collection =
              collections.find(collection => collection.id === collectionId);
          if (collection) {
            breadcrumbs.push(collection.name);
          }
        }
        break;
      case Paths.GooglePhotosCollection:
        breadcrumbs.push(this.i18n('googlePhotosLabel'));
        if (isNonEmptyString(googlePhotosAlbumId) &&
            isNonEmptyArray(googlePhotosAlbums)) {
          const googlePhotosAlbum = googlePhotosAlbums.find(
              googlePhotosAlbum =>
                  googlePhotosAlbum.id === googlePhotosAlbumId);
          if (googlePhotosAlbum) {
            breadcrumbs.push(googlePhotosAlbum.name);
          }
        }
        break;
      case Paths.LocalCollection:
        breadcrumbs.push(this.i18n('myImagesLabel'));
        break;
    }

    return breadcrumbs;
  }

  private computeShowBackButton_(path: string): boolean {
    return path !== Paths.Collections;
  }

  private getBackButtonAriaLabel_(): string {
    return this.i18n('back', this.i18n('title'));
  }

  private onBackClick_() {
    window.history.back();
  }

  private onBreadcrumbClick_(e: RepeaterEvent) {
    const index = e.model.index;
    const delta = this.breadcrumbs_.length - index - 1;
    if (delta > 0) {
      window.history.go(-delta);
    }
  }
}

customElements.define(PersonalizationBreadcrumb.is, PersonalizationBreadcrumb);
