// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays Google Photos photos.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './styles.js';
import '../../common/styles.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNumberOfGridItemsPerRow, isNonEmptyArray, isSelectionEvent, normalizeKeyForRTL} from '../../common/utils.js';
import {CurrentWallpaper, GooglePhotosPhoto, WallpaperImage, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isGooglePhotosPhoto} from '../utils.js';

import {selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

export interface GooglePhotosPhotos {
  $: {grid: IronListElement;};
}

export class GooglePhotosPhotos extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-photos';
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

      currentSelected_: Object,

      focusedColIndex_: {
        type: Number,
        value: 0,
      },

      pendingSelected_: Object,
      photos_: Array,

      photosByRow_: {
        type: Array,
        computed: 'computePhotosByRow_(photos_, photosLoading_, photosPerRow_)',
      },

      photosLoading_: Boolean,

      photosPerRow_: {
        type: Number,
        value: function() {
          return getNumberOfGridItemsPerRow();
        },
      },
    };
  }

  /** Whether or not this element is currently hidden. */
  hidden: boolean;

  /** The currently selected wallpaper. */
  private currentSelected_: CurrentWallpaper|null;

  /** The index of the currently focused column. */
  private focusedColIndex_: number;

  /** The pending selected wallpaper. */
  private pendingSelected_: FilePath|GooglePhotosPhoto|WallpaperImage|null;

  /** The list of photos. */
  private photos_: GooglePhotosPhoto[]|null|undefined;

  /**
   * The list of |photos_| split into the appropriate number of |photosPerRow_|
   * so as to be rendered in a grid.
   */
  private photosByRow_: GooglePhotosPhoto[][]|null;

  /** Whether the list of photos is currently loading. */
  private photosLoading_: boolean;

  /** The number of photos to render per row in a grid. */
  private photosPerRow_: number;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  connectedCallback() {
    super.connectedCallback();

    this.addEventListener('iron-resize', this.onResized_.bind(this));

    this.watch<GooglePhotosPhotos['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<GooglePhotosPhotos['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.watch<GooglePhotosPhotos['photos_']>(
        'photos_', state => state.wallpaper.googlePhotos.photos);
    this.watch<GooglePhotosPhotos['photosLoading_']>(
        'photosLoading_', state => state.wallpaper.loading.googlePhotos.photos);

    this.updateFromStore();
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_(hidden: GooglePhotosPhotos['hidden']) {
    if (hidden) {
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));
  }

  /** Invoked on focus of a grid row. */
  private onGridRowFocused_(e: Event) {
    // When a grid row is focused, forward the focus event on to the grid item
    // at the focused column index.
    const currentTarget = e.currentTarget as HTMLElement;
    const selector = `.photo[colindex="${this.focusedColIndex_}"]`;
    (currentTarget.querySelector(selector) as HTMLElement | null)?.focus();
  }

  /** Invoked on key down of a grid row. */
  private onGridRowKeyDown_(e: KeyboardEvent&{
    model: {index: number, row: GooglePhotosPhoto[]}
  }) {
    switch (normalizeKeyForRTL(e.key, this.i18n('textdirection') === 'rtl')) {
      case 'ArrowLeft':
        if (this.focusedColIndex_ > 0) {
          // Left arrow moves focus to the preceding grid item.
          this.focusedColIndex_ -= 1;
          this.$.grid.focusItem(e.model.index);
        } else if (e.model.index > 0) {
          // Left arrow moves focus to the preceding grid item, wrapping to the
          // preceding grid row.
          this.focusedColIndex_ = e.model.row.length - 1;
          this.$.grid.focusItem(e.model.index - 1);
        }
        return;
      case 'ArrowRight':
        if (this.focusedColIndex_ < e.model.row.length - 1) {
          // Right arrow moves focus to the succeeding grid item.
          this.focusedColIndex_ += 1;
          this.$.grid.focusItem(e.model.index);
        } else if (e.model.index < this.photosByRow_!.length - 1) {
          // Right arrow moves focus to the succeeding grid item, wrapping to
          // the succeeding grid row.
          this.focusedColIndex_ = 0;
          this.$.grid.focusItem(e.model.index + 1);
        }
        return;
      case 'Tab':
        // The grid contains a single |focusable| row which becomes a focus trap
        // due to the synthetic redirect of focus events to grid items. To
        // escape the trap, make the |focusable| row unfocusable until has
        // advanced to the next candidate.
        const focusable = this.$.grid.querySelector('[tabindex="0"]')!;
        focusable.setAttribute('tabindex', '-1');
        afterNextRender(this, () => focusable.setAttribute('tabindex', '0'));
        return;
    }
  }

  /** Invoked on selection of a photo. */
  private onPhotoSelected_(e: Event&{model: {photo: GooglePhotosPhoto}}) {
    assert(e.model.photo);
    if (isSelectionEvent(e)) {
      selectWallpaper(e.model.photo, this.wallpaperProvider_, this.getStore());
    }
  }

  /** Invoked on resize of this element. */
  private onResized_() {
    this.photosPerRow_ = getNumberOfGridItemsPerRow();
  }

  /** Invoked to compute |photosByRow_|. */
  private computePhotosByRow_(
      photos: GooglePhotosPhotos['photos_'],
      photosLoading: GooglePhotosPhotos['photosLoading_'],
      photosPerRow: GooglePhotosPhotos['photosPerRow_']):
      GooglePhotosPhoto[][]|null {
    if (photosLoading || !photosPerRow) {
      return null;
    }
    if (!isNonEmptyArray(photos)) {
      return null;
    }
    return Array.from(
        {length: Math.ceil(photos.length / photosPerRow)}, (_, i) => {
          i *= photosPerRow;
          return photos!.slice(i, i + photosPerRow);
        });
  }

  // Returns whether the specified |photo| is currently selected.
  private isPhotoSelected_(
      photo: GooglePhotosPhoto|null,
      currentSelected: GooglePhotosPhotos['currentSelected_'],
      pendingSelected: GooglePhotosPhotos['pendingSelected_']): boolean {
    if (!photo || (!currentSelected && !pendingSelected)) {
      return false;
    }
    if (isGooglePhotosPhoto(pendingSelected) &&
        pendingSelected!.id === photo.id) {
      return true;
    }
    if (!pendingSelected &&
        currentSelected?.type === WallpaperType.kGooglePhotos &&
        currentSelected!.key === photo.id) {
      return true;
    }
    return false;
  }
}

customElements.define(GooglePhotosPhotos.is, GooglePhotosPhotos);
