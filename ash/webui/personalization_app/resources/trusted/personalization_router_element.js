// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This router component hooks into the current url path and query
 * parameters to display sections of the personalization SWA.
 */

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
export const Paths = {
  CollectionImages: '/wallpaper/collection',
  Collections: '/wallpaper',
  GooglePhotosCollection: '/wallpaper/google-photos',
  LocalCollection: '/wallpaper/local',
  Root: '/',
};

export class PersonalizationRouter extends PolymerElement {
  static get is() {
    return 'personalization-router';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      path_: {
        type: String,
      },

      /** @private */
      query_: {
        type: String,
      },

      /** @private */
      queryParams_: {
        type: Object,
      },
    };
  }

  static instance() {
    return document.querySelector(PersonalizationRouter.is);
  }

  /**
   * Reload the application at the collections page.
   */
  static reloadAtWallpaper() {
    window.location.replace(Paths.Collections);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    // Force the user onto the wallpaper subpage if personalization hub feature
    // is not enabled, and the user is not already on a wallpaper page.
    if (!loadTimeData.getBoolean('isPersonalizationHubEnabled') &&
        !this.shouldShowWallpaperSubpage_(this.path_)) {
      PersonalizationRouter.reloadAtWallpaper();
    }
  }

  get collectionId() {
    if (this.path_ !== Paths.CollectionImages) {
      return null;
    }
    return this.queryParams_.id;
  }

  /**
   * Navigate to the selected collection id. Assumes validation of the
   * collection has already happened.
   * @param {!WallpaperCollection} collection
   */
  selectCollection(collection) {
    document.title = collection.name;
    this.setProperties(
        {path_: Paths.CollectionImages, queryParams_: {id: collection.id}});
  }

  /**
   * Navigate to the Google Photos collection page.
   */
  selectGooglePhotosCollection() {
    this.setProperties({path_: Paths.GooglePhotosCollection, query_: ''});
  }

  /**
   * Navigate to the local collection page.
   */
  selectLocalCollection() {
    this.setProperties({path_: Paths.LocalCollection, query_: ''});
  }

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  shouldShowCollections_(path) {
    return path === Paths.Collections;
  }

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  shouldShowCollectionImages_(path) {
    return path === Paths.CollectionImages;
  }

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  shouldShowGooglePhotosCollection_(path) {
    return path === Paths.GooglePhotosCollection;
  }

  /**
   * @param {string} path
   * @return  {boolean}
   * @private
   */
  shouldShowLocalCollection_(path) {
    return path === Paths.LocalCollection;
  }

  /**
   * Whether Google Photos integration is enabled.
   * @return {boolean}
   * @private
   */
  isGooglePhotosIntegrationEnabled_() {
    return loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled');
  }

  /**
   * @param {?string} path
   * @return {boolean}
   * @private
   */
  shouldShowRootPage_(path) {
    return loadTimeData.getBoolean('isPersonalizationHubEnabled') &&
        path === Paths.Root;
  }

  /**
   * @param {?string} path
   * @return {boolean}
   * @private
   */
  shouldShowWallpaperSubpage_(path) {
    return !!path?.startsWith(Paths.Collections);
  }
}

customElements.define(PersonalizationRouter.is, PersonalizationRouter);
