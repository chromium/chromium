// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays Google Photos photos.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import './styles.js';
import '../../common/styles.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNumberOfGridItemsPerRow, isNonEmptyArray, isSelectionEvent, normalizeKeyForRTL} from '../../common/utils.js';
import {CurrentWallpaper, GooglePhotosPhoto, WallpaperImage, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isGooglePhotosPhoto} from '../utils.js';

import {fetchGooglePhotosPhotos, selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

/** A list of |GooglePhotosPhoto|'s to be rendered in a row. */
export type GooglePhotosPhotosRow = GooglePhotosPhoto[];

/** A titled list of |GooglePhotosPhotosRow|'s to be rendered in a section. */
export type GooglePhotosPhotosSection = {
  title: string,
  rows: GooglePhotosPhotosRow[]
};

export interface GooglePhotosPhotos {
  $: {grid: IronListElement; gridScrollThreshold: IronScrollThresholdElement};
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
      photosByRow_: Array,

      photosBySection_: {
        type: Array,
        computed: 'computePhotosBySection_(photos_, photosPerRow_)',
        observer: 'onPhotosBySectionChanged_',
      },

      photosLoading_: Boolean,

      photosPerRow_: {
        type: Number,
        value: function() {
          return getNumberOfGridItemsPerRow();
        },
      },

      photosResumeToken_: {
        type: String,
        observer: 'onPhotosResumeTokenChanged_',
      },
    };
  }

  /** Whether or not this element is currently hidden. */
  override hidden: boolean;

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

  /**
   * The list of |photos_| split into the appropriate number of |photosPerRow_|
   * and grouped into sections so as to be rendered in a grid.
   */
  private photosBySection_: GooglePhotosPhotosSection[]|null;

  /** Whether the list of photos is currently loading. */
  private photosLoading_: boolean;

  /** The number of photos to render per row in a grid. */
  private photosPerRow_: number;

  /** The resume token needed to fetch the next page of photos. */
  private photosResumeToken_: string|null;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  override connectedCallback() {
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
    this.watch<GooglePhotosPhotos['photosResumeToken_']>(
        'photosResumeToken_',
        state => state.wallpaper.googlePhotos.resumeTokens.photos);

    this.updateFromStore();
  }

  /** Invoked on grid scroll threshold reached. */
  private onGridScrollThresholdReached_() {
    // Ignore this event if fired during initialization.
    if (!this.$.gridScrollThreshold.scrollHeight) {
      this.$.gridScrollThreshold.clearTriggers();
      return;
    }

    // Ignore this event if photos are already being loading or if there is no
    // resume token (indicating there are no additional photos to load).
    if (this.photosLoading_ === true || this.photosResumeToken_ === null) {
      return;
    }

    // Fetch the next page of photos.
    fetchGooglePhotosPhotos(this.wallpaperProvider_, this.getStore());
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

  /** Invoked on selection of a photo. */
  private onPhotoSelected_(e: Event&{model: {photo: GooglePhotosPhoto}}) {
    assert(e.model.photo);
    if (isSelectionEvent(e)) {
      selectWallpaper(e.model.photo, this.wallpaperProvider_, this.getStore());
    }
  }

  /** Invoked on changes to |photosBySection_|. */
  private onPhotosBySectionChanged_(
      photosBySection: GooglePhotosPhotos['photosBySection_']) {
    if (!isNonEmptyArray(photosBySection)) {
      this.photosByRow_ = null;
      return;
    }

    const photosByRow = photosBySection.flatMap(section => section.rows);

    // Case: First batch of photos.
    if (this.photosByRow_ === null || this.photosByRow_ === undefined) {
      this.photosByRow_ = photosByRow;
      return;
    }

    // Case: Subsequent batches of photos.
    // NOTE: |photosByRow_| is updated in place to avoid resetting the scroll
    // position of the grid.
    photosByRow.forEach((row, i) => {
      if (i < this.photosByRow_!.length) {
        this.set(`photosByRow_.${i}`, row);
      } else {
        this.push('photosByRow_', row);
      }
    });

    while (this.photosByRow_.length > photosByRow.length) {
      this.pop('photosByRow_');
    }
  }

  /** Invoked on changes to |photosResumeToken_|. */
  private onPhotosResumeTokenChanged_(
      photosResumeToken: GooglePhotosPhotos['photosResumeToken_']) {
    if (photosResumeToken?.length) {
      this.$.gridScrollThreshold.clearTriggers();
    }
  }

  /** Invoked on resize of this element. */
  private onResized_() {
    this.photosPerRow_ = getNumberOfGridItemsPerRow();
  }

  /** Invoked to compute |photosBySection_|. */
  private computePhotosBySection_(
      photos: GooglePhotosPhotos['photos_'],
      photosPerRow: GooglePhotosPhotos['photosPerRow_']):
      GooglePhotosPhotosSection[]|null {
    if (!isNonEmptyArray(photos) || !photosPerRow) {
      return null;
    }

    const sections: GooglePhotosPhotosSection[] = [];

    photos.forEach(photo => {
      const title = photo.date.data.map(c => String.fromCodePoint(c)).join('');

      // Find/create the appropriate |section| in which to insert |photo|.
      let section = sections[sections.length - 1];
      if (section?.title !== title) {
        section = {title, rows: []};
        sections.push(section);
      }

      // Find/create the appropriate |row| in which to insert |photo|.
      let row = section.rows[section.rows.length - 1];
      if ((row?.length ?? photosPerRow) === photosPerRow) {
        row = [];
        section.rows.push(row);
      }

      row.push(photo);
    });

    return sections;
  }

  // Returns the title to display for the specified grid |row|.
  private getGridRowTitle_(
      row: GooglePhotosPhotosRow,
      photosBySection: GooglePhotosPhotos['photosBySection_']): string
      |undefined {
    return photosBySection?.find(section => section.rows[0] === row)?.title;
  }

  // Returns whether the title for the specified grid |row| is visible.
  private isGridRowTitleVisible_(
      row: GooglePhotosPhotosRow,
      photosBySection: GooglePhotosPhotos['photosBySection_']): boolean {
    return !!this.getGridRowTitle_(row, photosBySection);
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
