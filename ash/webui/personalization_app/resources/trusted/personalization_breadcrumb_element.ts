// Copyright 2022 The Chromium Authors. All rights reserved.
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

import {GooglePhotosAlbum, WallpaperCollection} from './personalization_app.mojom-webui.js';
import {isPersonalizationHubEnabled, Paths, PersonalizationRouter} from './personalization_router_element.js';
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

      /** The current Google Photos album id to display. */
      googlePhotosAlbumId: String,

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

      /** The list of Google Photos albums. */
      googlePhotosAlbums_: Array,

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
  private googlePhotosAlbums_: GooglePhotosAlbum[]|null;
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
      collectionId: string, googlePhotosAlbums: GooglePhotosAlbum[]|null,
      googlePhotosAlbumId: string|null): string[] {
    const breadcrumbs = [];
    switch (path) {
      case Paths.Collections:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        break;
      case Paths.CollectionImages:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        if (isNonEmptyArray(collections)) {
          const collection =
              collections.find(collection => collection.id === collectionId);
          if (collection) {
            breadcrumbs.push(collection.name);
          }
        }
        break;
      case Paths.GooglePhotosCollection:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('googlePhotosLabel'));
        if (isNonEmptyString(googlePhotosAlbumId) &&
            isNonEmptyArray(googlePhotosAlbums)) {
          const googlePhotosAlbum = googlePhotosAlbums.find(
              googlePhotosAlbum =>
                  googlePhotosAlbum.id === googlePhotosAlbumId);
          if (googlePhotosAlbum) {
            breadcrumbs.push(googlePhotosAlbum.title);
          }
        }
        break;
      case Paths.LocalCollection:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('myImagesLabel'));
        break;
      case Paths.User:
        breadcrumbs.push(this.i18n('avatarLabel'));
        break;
      case Paths.Ambient:
        breadcrumbs.push(this.i18n('screensaverLabel'));
        break;
    }
    return breadcrumbs;
  }

  private computeShowBackButton_(path: string): boolean {
    // Do not show the back button if hub is enabled.
    return !isPersonalizationHubEnabled() && path !== Paths.Collections;
  }

  private showHomeButton_(): boolean {
    return isPersonalizationHubEnabled();
  }

  private getBackButtonAriaLabel_(): string {
    return this.i18n('back', this.i18n('wallpaperLabel'));
  }

  private onBackClick_() {
    window.history.back();
  }

  private onBreadcrumbClick_(e: RepeaterEvent) {
    const index = e.model.index;
    // stay in same page if the user clicks on the last breadcrumb,
    // else navigate to the corresponding page.
    if (index < this.breadcrumbs_.length - 1) {
      const pathElements = this.path.split('/');
      const newPath = pathElements.slice(0, index + 2).join('/');
      if (Object.values(Paths).includes(newPath as Paths)) {
        PersonalizationRouter.instance().goToRoute(newPath as Paths);
      }
    }
  }

  private onHomeIconClick_() {
    PersonalizationRouter.instance().goToRoute(Paths.Root);
  }
}

customElements.define(PersonalizationBreadcrumb.is, PersonalizationBreadcrumb);
