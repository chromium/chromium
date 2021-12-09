// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays Google Photos albums.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './styles.js';
import '/common/styles.js';

import {isSelectionEvent} from '/common/utils.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

/** @polymer */
export class GooglePhotosAlbums extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-albums';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Whether or not this element is currently hidden.
       * @type {boolean}
       */
      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },

      /**
       * The list of albums.
       * @type {?Array<WallpaperCollection>}
       * @private
       */
      albums_: {
        type: Array,
      },

      /**
       * Whether the list of albums is currently loading.
       * @type {boolean}
       * @private
       */
      albumsLoading_: {
        type: Boolean,
      },
    };
  }

  static get observers() {
    return [
      'onHiddenChanged_(hidden)',
    ];
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.watch('albums_', state => state.googlePhotos.albums);
    this.watch('albumsLoading_', state => state.loading.googlePhotos.albums);

    this.updateFromStore();
  }

  /**
   * Invoked on selection of an album.
   * @param {!Event} e
   * @private
   */
  onAlbumSelected_(e) {
    assert(e.model.album);
    if (isSelectionEvent(e)) {
      PersonalizationRouter.instance().selectGooglePhotosAlbum(e.model.album);
    }
  }

  /**
   * Invoked on changes to this element's hidden state.
   * @private
   */
  onHiddenChanged_() {
    if (this.hidden) {
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => {
      this.shadowRoot.querySelector('iron-list').fire('iron-resize');
    });
  }
}

customElements.define(GooglePhotosAlbums.is, GooglePhotosAlbums);
