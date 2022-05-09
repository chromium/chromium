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

import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DisplayableImage} from '../../common/constants.js';
import {getLoadingPlaceholders, getNumberOfGridItemsPerRow, isNonEmptyArray, isSelectionEvent, normalizeKeyForRTL} from '../../common/utils.js';
import {dismissErrorAction, setErrorAction} from '../personalization_actions.js';
import {CurrentWallpaper, GooglePhotosPhoto, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isGooglePhotosPhoto} from '../utils.js';

import {recordWallpaperGooglePhotosSourceUMA, WallpaperGooglePhotosSource} from './google_photos_metrics_logger.js';
import {getTemplate} from './google_photos_photos_element.html.js';
import {fetchGooglePhotosPhotos, selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const ERROR_ID = 'GooglePhotosPhotos';
const PLACEHOLDER_ID = 'placeholder';

/** Returns placeholders to show while Google Photos photos are loading. */
function getPlaceholders(): GooglePhotosPhotosRow[] {
  const placeholdersPerRow = getNumberOfGridItemsPerRow();
  const placeholders: GooglePhotosPhotosRow[] = [];
  getLoadingPlaceholders(() => {
    const photo = new GooglePhotosPhoto();
    photo.id = PLACEHOLDER_ID;
    return photo;
  }).forEach((placeholder, i) => {
    let row = placeholders[placeholders.length - 1];
    if (!row || row.length === placeholdersPerRow) {
      row = [];
      placeholders.push(row);
    }
    row.push({...placeholder, index: i});
  });
  return placeholders;
}

/** A single |GooglePhotosPhoto| coupled with its numerical index. */
export type GooglePhotosPhotoWithIndex = GooglePhotosPhoto&{index: number};

/** A list of |GooglePhotosPhotoWithIndex|'s to be rendered in a row. */
export type GooglePhotosPhotosRow = GooglePhotosPhotoWithIndex[];

/** A titled list of |GooglePhotosPhotosRow|'s to be rendered in a section. */
export type GooglePhotosPhotosSection = {
  date: string,
  locations: Set<string>,
  rows: GooglePhotosPhotosRow[],
};

export interface GooglePhotosPhotos {
  $: {grid: IronListElement, gridScrollThreshold: IronScrollThresholdElement};
}

export class GooglePhotosPhotos extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-photos';
  }

  static get template() {
    return getTemplate();
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
        value: getPlaceholders,
      },

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
        observer: 'onPhotosPerRowChanged_',
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
  private pendingSelected_: DisplayableImage|null;

  /** The list of photos. */
  private photos_: GooglePhotosPhoto[]|null|undefined;

  /**
   * The list of |photos_| split into the appropriate number of |photosPerRow_|
   * so as to be rendered in a grid.
   */
  private photosByRow_: GooglePhotosPhotosRow[];

  /**
   * The list of |photos_| split into the appropriate number of |photosPerRow_|
   * and grouped into sections so as to be rendered in a grid.
   */
  private photosBySection_: GooglePhotosPhotosSection[]|null|undefined;

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
    if (this.photosLoading_ === true || !this.photosResumeToken_) {
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
    const element = currentTarget.querySelector(selector) as HTMLElement;
    if (element) {
      element.focus();
    }
  }

  /** Invoked on key down of a grid row. */
  private onGridRowKeyDown_(e: KeyboardEvent&{
    model: {index: number, row: GooglePhotosPhoto[]},
  }) {
    switch (normalizeKeyForRTL(e.key, this.i18n('textdirection') === 'rtl')) {
      case 'ArrowDown':
        if (e.model.index < this.photosByRow_!.length - 1) {
          // To be consistent with default iron-list grid behavior, the down
          // arrow should only advance focus to the succeeding grid row if an
          // item at the same column index as is currently focused exists.
          const nextGridRow = this.photosByRow_[e.model.index + 1];
          if (this.focusedColIndex_ >= nextGridRow.length) {
            e.preventDefault();
            e.stopPropagation();
          }
        }
        return;
      case 'ArrowLeft':
        if (this.focusedColIndex_ > 0) {
          // Left arrow moves focus to the preceding grid item.
          this.focusedColIndex_ -= 1;
          this.$.grid.focusItem(e.model.index);
        } else if (e.model.index > 0) {
          // Left arrow moves focus to the preceding grid item, wrapping to the
          // preceding grid row.
          const previousGridRow = this.photosByRow_[e.model.index - 1];
          this.focusedColIndex_ = previousGridRow.length - 1;
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
      case 'ArrowUp':
        if (e.model.index > 0) {
          // To be consistent with default iron-list grid behavior, the up arrow
          // should only advance focus to the preceding grid row if an item at
          // the same column index as is currently focused exists.
          const previousGridRow = this.photosByRow_[e.model.index - 1];
          if (this.focusedColIndex_ >= previousGridRow.length) {
            e.preventDefault();
            e.stopPropagation();
          }
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
      // If |hidden|, the error associated with this element will have lost
      // user-facing context so it should be dismissed.
      this.dispatch(dismissErrorAction(ERROR_ID, /*fromUser=*/ false));
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));

    // When showing the user a list of photos that previously failed to load,
    // we should automatically retry loading the list. Placeholders should be
    // shown while loading is in progress.
    if (this.photos_ === null && !this.photosLoading_) {
      fetchGooglePhotosPhotos(this.wallpaperProvider_, this.getStore());
      this.photosByRow_ = getPlaceholders();
    }
  }

  /** Invoked on selection of a photo. */
  private onPhotoSelected_(e: Event&{model: {photo: GooglePhotosPhoto}}) {
    assert(e.model.photo);
    if (!this.isPhotoPlaceholder_(e.model.photo) && isSelectionEvent(e)) {
      selectWallpaper(e.model.photo, this.wallpaperProvider_, this.getStore());
      recordWallpaperGooglePhotosSourceUMA(WallpaperGooglePhotosSource.PHOTOS);
    }
  }

  /** Invoked on changes to |photosBySection_|. */
  private onPhotosBySectionChanged_(
      photosBySection: GooglePhotosPhotos['photosBySection_']) {
    if (photosBySection === null) {
      // If the list of photos fails to load and is currently showing, display
      // an error to the user that allows them to make another attempt.
      if (!this.hidden) {
        this.dispatch(setErrorAction({
          id: ERROR_ID,
          message: this.i18n('googlePhotosError'),
          dismiss: {
            message: this.i18n('googlePhotosRetry'),
            callback: (fromUser: boolean) => {
              if (fromUser) {
                // Post the reattempt instead of performing it immediately to
                // avoid updating the personalization store from the same
                // sequence that generated this event.
                setTimeout(
                    () => fetchGooglePhotosPhotos(
                        this.wallpaperProvider_, this.getStore()));
              }
            },
          },
        }));
      }
      // Whether the list of photos is currently showing or not, placeholders
      // should not be removed on load failure.
      return;
    }

    // NOTE: |photosByRow_| is updated in place to avoid resetting the scroll
    // position of the grid which would otherwise occur during reassignment.
    this.updateList(
        /*propertyPath=*/ 'photosByRow_', /*identityGetter=*/
        (row: GooglePhotosPhotosRow) => row.map(photo => photo.id).join('_'),
        /*newList=*/
        Array.isArray(photosBySection) ?
            photosBySection.flatMap(section => section.rows) :
            [],
        /*identityBasedUpdate=*/ true);
  }

  /** Invoked on changes to |photosPerRow_|. */
  private onPhotosPerRowChanged_(photosPerRow:
                                     GooglePhotosPhotos['photosPerRow_']) {
    // Because this element manually partitions photos by row, placeholders need
    // to be explicitly regenerated when the desired number of photos per row
    // changes.
    if (photosPerRow && isNonEmptyArray(this.photosByRow_) &&
        this.photosByRow_.every(r => r.every(p => p.id === PLACEHOLDER_ID))) {
      this.photosByRow_ = getPlaceholders();
    }
  }

  /** Invoked on changes to |photosResumeToken_|. */
  private onPhotosResumeTokenChanged_(
      photosResumeToken: GooglePhotosPhotos['photosResumeToken_']) {
    if (photosResumeToken) {
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
      GooglePhotosPhotosSection[]|null|undefined {
    // If |photos| is undefined, this computation is occurring during
    // initialization. In such cases, defer defining |photosBySection_| until
    // actual data has loaded or failed to load.
    if (photos === undefined) {
      return undefined;
    }

    if (!Array.isArray(photos) || !photosPerRow) {
      return null;
    }

    const sections: GooglePhotosPhotosSection[] = [];

    photos.forEach((photo, i) => {
      const date = photo.date.data.map(c => String.fromCodePoint(c)).join('');

      // Find/create the appropriate |section| in which to insert |photo|.
      let section = sections[sections.length - 1];
      if (!section || section.date !== date) {
        section = {date, locations: new Set<string>(), rows: []};
        sections.push(section);
      }

      // Find/create the appropriate |row| in which to insert |photo|.
      let row = section.rows[section.rows.length - 1];
      if (!row || row.length === photosPerRow) {
        row = [];
        section.rows.push(row);
      }

      row.push({...photo, index: i});

      if (photo.location) {
        section.locations.add(photo.location);
      }
    });

    return sections;
  }

  /** Returns the date to display for the specified grid |row|. */
  private getGridRowDate_(
      row: GooglePhotosPhotosRow,
      photosBySection: GooglePhotosPhotos['photosBySection_']): string
      |undefined {
    if (!photosBySection) {
      return undefined;
    }
    const gridRowSection =
        photosBySection.find(section => section.rows[0] === row);
    return gridRowSection ? gridRowSection.date : undefined;
  }

  /** Returns the locations to display for the specified grid |row|. */
  private getGridRowLocations_(
      row: GooglePhotosPhotosRow,
      photosBySection: GooglePhotosPhotos['photosBySection_']): string
      |undefined {
    if (!photosBySection) {
      return undefined;
    }
    const gridRowSection =
        photosBySection.find(section => section.rows[0] === row);
    return gridRowSection ?
        Array.from(gridRowSection.locations).sort().join(' · ') :
        undefined;
  }

  /** Returns the aria label for the specified |photo|. */
  private getPhotoAriaLabel_(photo: GooglePhotosPhoto|null): string|undefined {
    if (photo) {
      return photo.id === PLACEHOLDER_ID ? this.i18n('ariaLabelLoading') :
                                           photo.name;
    }
    return undefined;
  }

  /** Returns whether the title for the specified grid |row| is visible. */
  private isGridRowTitleVisible_(
      row: GooglePhotosPhotosRow,
      photosBySection: GooglePhotosPhotos['photosBySection_']): boolean {
    return !!this.getGridRowDate_(row, photosBySection);
  }

  /** Returns whether the specified |photo| is a placeholder. */
  private isPhotoPlaceholder_(photo: GooglePhotosPhoto|null): boolean {
    return !!photo && photo.id === PLACEHOLDER_ID;
  }

  /** Returns whether the specified |photo| is currently selected. */
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
    if (!pendingSelected && !!currentSelected &&
        currentSelected.type === WallpaperType.kOnceGooglePhotos &&
        currentSelected.key === photo.id) {
      return true;
    }
    return false;
  }
}

customElements.define(GooglePhotosPhotos.is, GooglePhotosPhotos);
