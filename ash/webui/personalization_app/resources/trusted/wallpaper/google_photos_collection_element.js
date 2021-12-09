// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that fetches and displays the Google Photos
 * collection.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './styles.js';
import '/common/styles.js';

import {getNumberOfGridItemsPerRow, isNonEmptyArray, isSelectionEvent, normalizeKeyForRTL} from '/common/utils.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {initializeGooglePhotosData} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

/**
 * Enumeration of supported tabs.
 * @enum {string}
 */
const Tab = {
  Albums: 'albums',
  Photos: 'photos',
};

/** @polymer */
export class GooglePhotosCollection extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-collection';
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
       * The list of photos.
       * @type {?Array<undefined>}
       * @private
       */
      photos_: {
        type: Array,
      },

      /**
       * The list of |photos_| split into the appropriate number of
       * |photosPerRow_| so as to be rendered in a grid.
       * @type {?Array<Array<undefined>>}
       * @private
       */
      photosByRow_: {
        type: Array,
      },

      /**
       * The index of the currently focused column in the photos grid.
       * @type {number}
       * @private
       */
      photosGridFocusedColIndex_: {
        type: Number,
        value: 0,
      },

      /**
       * Whether the list of photos is currently loading.
       * @type {boolean}
       * @private
       */
      photosLoading_: {
        type: Boolean,
      },

      /**
       * The number of photos to render per row in a grid.
       * @type {number}
       * @private
       */
      photosPerRow_: {
        type: Number,
        value: function() {
          return getNumberOfGridItemsPerRow();
        },
      },

      /**
       * The currently selected tab.
       * @type {!Tab}
       * @private
       */
      tab_: {
        type: String,
        value: Tab.Photos,
      },
    };
  }

  static get observers() {
    return [
      'onHiddenChanged_(hidden)',
      'onPhotosChanged_(photos_, photosLoading_, photosPerRow_)',
    ];
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

    this.addEventListener('iron-resize', this.onResized_.bind(this));

    this.watch('albums_', state => state.googlePhotos.albums);
    this.watch('photos_', state => state.googlePhotos.photos);
    this.watch('photosLoading_', state => state.loading.googlePhotos.photos);
    this.updateFromStore();

    initializeGooglePhotosData(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Invoked on changes to this element's hidden state.
   * @private
   */
  onHiddenChanged_() {
    if (this.hidden) {
      return;
    }

    document.title = this.i18n('googlePhotosLabel');
    this.shadowRoot.getElementById('main').focus();

    // When grid items change while their parent element is hidden, the grid
    // will render incorrectly. Force relayout by invalidating the grid for the
    // currently selected tab when this element becomes visible.
    afterNextRender(this, () => this.invalidateGrid_());
  }

  /**
   * Invoked on changes to the list of photos, its loading state, and the
   * number of photos to render per row in a grid.
   * @param {?Array<undefined>} photos
   * @param {?boolean} photosLoading
   * @param {?number} photosPerRow
   * @private
   */
  onPhotosChanged_(photos, photosLoading, photosPerRow) {
    if (photosLoading || !photosPerRow) {
      return;
    }
    if (!isNonEmptyArray(photos)) {
      this.photosByRow_ = null;
      return;
    }
    let index = 0;
    this.photosByRow_ = Array.from(
        {length: Math.ceil(photos.length / photosPerRow)}, (_, i) => {
          i *= photosPerRow;
          const row = photos.slice(i, i + photosPerRow).map(photo => index++);
          while (row.length < photosPerRow) {
            row.push(undefined);
          }
          return row;
        });
  }

  /**
   * Invoked on focus of a photos grid row.
   * @param {!Event } e
   * @private
   */
  onPhotosGridRowFocused_(e) {
    // When a grid row is focused, forward the focus event on to the grid item
    // at the focused column index.
    const selector = `.photo[colindex="${this.photosGridFocusedColIndex_}"]`;
    e.currentTarget.querySelector(selector)?.focus();
  }

  /**
   * Invoked on key down of a photos grid row.
   * @param {!Event} e
   * @private
   */
  onPhotosGridRowKeyDown_(e) {
    const row = (/** @type {{row: Array<undefined>}} */ (e.model)).row;

    switch (normalizeKeyForRTL(e.key, this.i18n('textdirection') === 'rtl')) {
      case 'ArrowLeft':
        if (this.photosGridFocusedColIndex_ > 0) {
          // Left arrow moves focus to the preceding grid item.
          this.photosGridFocusedColIndex_ -= 1;
          this.$.photosGrid.focusItem(e.model.index);
        } else if (e.model.index > 0) {
          // Left arrow moves focus to the preceding grid item, wrapping to the
          // preceding grid row.
          this.photosGridFocusedColIndex_ = row.length - 1;
          this.$.photosGrid.focusItem(e.model.index - 1);
        }
        return;
      case 'ArrowRight':
        if (this.photosGridFocusedColIndex_ < row.length - 1) {
          // Right arrow moves focus to the succeeding grid item.
          this.photosGridFocusedColIndex_ += 1;
          this.$.photosGrid.focusItem(e.model.index);
        } else if (e.model.index < this.photosByRow_.length - 1) {
          // Right arrow moves focus to the succeeding grid item, wrapping to
          // the succeeding grid row.
          this.photosGridFocusedColIndex_ = 0;
          this.$.photosGrid.focusItem(e.model.index + 1);
        }
        return;
      case 'Tab':
        // The grid contains a single |focusable| row which becomes a focus trap
        // due to the synthetic redirect of focus events to grid items. To
        // escape the trap, make the |focusable| row unfocusable until has
        // advanced to the next candidate.
        const focusable = this.$.photosGrid.querySelector('[tabindex="0"]');
        focusable.setAttribute('tabindex', -1);
        afterNextRender(focusable, () => focusable.setAttribute('tabindex', 0));
        return;
    }
  }

  /**
   * Invoked on resize of this element.
   * @private
   */
  onResized_() {
    this.photosPerRow_ = getNumberOfGridItemsPerRow();
  }

  /**
   * Invoked on tab selected.
   * @param {!Event} e
   * @private
   */
  onTabSelected_(e) {
    const previousTab = this.tab_;

    switch (e.currentTarget.id) {
      case 'albumsTab':
        this.tab_ = Tab.Albums;
        break;
      case 'photosTab':
        this.tab_ = Tab.Photos;
        break;
      default:
        assertNotReached();
        break;
    }

    // When grid items change while their parent element is hidden, the grid
    // will render incorrectly. Force relayout by invalidating the grid for the
    // currently selected tab when it becomes visible.
    if (this.tab_ !== previousTab) {
      afterNextRender(this, () => this.invalidateGrid_());
    }
  }

  /**
   * Invalidates the grid for the currently selected tab to force relayout.
   * @private
   */
  invalidateGrid_() {
    switch (this.tab_) {
      case Tab.Albums:
        // Firing 'iron-resize' event forces relayout of 'iron-list'.
        this.shadowRoot.querySelector('#albumsGrid')?.fire('iron-resize');
        return;
      case Tab.Photos:
        // Firing 'iron-resize' event forces relayout of 'iron-list'.
        this.shadowRoot.querySelector('#photosGrid')?.fire('iron-resize');
        return;
      default:
        assertNotReached();
        return;
    }
  }

  /**
   * Whether the list of albums is empty.
   * @return {boolean}
   * @private
   */
  isAlbumsEmpty_() {
    return !isNonEmptyArray(this.albums_);
  }

  /**
   * Whether the albums tab is currently selected.
   * @return {boolean}
   * @private
   */
  isAlbumsTabSelected_() {
    return this.tab_ === Tab.Albums;
  }

  /**
   * Whether the list of photos is empty.
   * @return {boolean}
   * @private
   */
  isPhotosEmpty_() {
    return !isNonEmptyArray(this.photos_);
  }

  /**
   * Whether the photos tab is currently selected.
   * @return {boolean}
   * @private
   */
  isPhotosTabSelected_() {
    return this.tab_ === Tab.Photos;
  }
}

customElements.define(GooglePhotosCollection.is, GooglePhotosCollection);
