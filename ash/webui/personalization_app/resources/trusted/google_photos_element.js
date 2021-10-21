// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays the Google Photos
 * collection.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './styles.js';
import '../common/styles.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {initializeGooglePhotosData} from './personalization_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';

/** @polymer */
export class GooglePhotos extends WithPersonalizationStore {
  static get is() {
    return 'google-photos';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The list of photos.
       * @type {?Array<undefined>}
       * @private
       */
      photos_: {
        type: Array,
      },

      /**
       * Whether the list of photos is currently loading.
       * @type {boolean}
       * @private
       */
      photosLoading_: {
        type: Boolean,
      }
    };
  }

  static get observers() {
    return ['onPhotosLoaded_(photos_, photosLoading_)'];
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

    this.watch('photos_', state => state.googlePhotos.photos);
    this.watch('photosLoading_', state => state.loading.googlePhotos.photos);
    this.updateFromStore();

    initializeGooglePhotosData(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Invoked on changes to the list of photos and its loading state.
   * @param {?Array<undefined>} photos
   * @param {boolean} photosLoading
   * @private
   */
  onPhotosLoaded_(photos, photosLoading) {
    // TODO(dmblack): Send event to untrusted via iframe API.
  }
}

customElements.define(GooglePhotos.is, GooglePhotos);
