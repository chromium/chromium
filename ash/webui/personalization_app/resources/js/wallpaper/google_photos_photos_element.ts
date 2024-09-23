// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays Google Photos photos.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';

import {WallpaperGridItemSelectedEvent} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CurrentWallpaper, GooglePhotosPhoto, WallpaperProviderInterface, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {dismissErrorAction, setErrorAction} from '../personalization_actions.js';
import {PersonalizationStateError} from '../personalization_state.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getNumberOfGridItemsPerRow} from '../utils.js';

import {DisplayableImage} from './constants.js';
import {recordWallpaperGooglePhotosSourceUMA, WallpaperGooglePhotosSource} from './google_photos_metrics_logger.js';
import {getTemplate} from './google_photos_photos_element.html.js';
import {getLoadingPlaceholders, isGooglePhotosPhoto, isImageAMatchForKey, isImageEqualToSelected} from './utils.js';
import {fetchGooglePhotosPhotos, selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const ERROR_ID = 'GooglePhotosPhotos';
const PLACEHOLDER_ID = 'placeholder';

/** Returns placeholders to show while Google Photos photos are loading. */
function getPlaceholders(): GooglePhotosPhotosRow[] {
  const placeholdersPerRow = getNumberOfGridItemsPerRow();
  const placeholders: GooglePhotosPhotosRow[] = [];
  getLoadingPlaceholders(() => {
    return {
      id: PLACEHOLDER_ID,
      name: '',
      date: {data: []},
      url: {url: ''},
      dedupKey: null,
      location: null,
    };
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

/**
 * Normalizes the given |key| for RTL.
 */
export function normalizeKeyForRTL(key: string, isRTL: boolean): string {
  if (isRTL) {
    if (key === 'ArrowLeft') {
      return 'ArrowRight';
    }
    if (key === 'ArrowRight') {
      return 'ArrowLeft';
    }
  }
  return key;
}

/** A single |GooglePhotosPhoto| coupled with its numerical index. */
export type GooglePhotosPhotoWithIndex = GooglePhotosPhoto&{index: number};

/** A list of |GooglePhotosPhotoWithIndex|'s to be rendered in a row. */
export type GooglePhotosPhotosRow = GooglePhotosPhotoWithIndex[];

/** A titled list of |GooglePhotosPhotosRow|'s to be rendered in a section. */
export interface GooglePhotosPhotosSection {
  date: string;
  locations: Set<string>;
  rows: GooglePhotosPhotosRow[];
}

export interface GooglePhotosPhotosElement {
  $: {grid: IronListElement, gridScrollThreshold: IronScrollThresholdElement};
}

export class GooglePhotosPhotosElement extends WithPersonalizationStore {
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

      focusedPhotoIndex_: {
        type: Number,
        value: -1,
        observer: 'onFocusedPhotoIndexChanged_',
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

      error_: {
        type: Object,
        value: null,
      },
    };
  }

  /** Whether or not this element is currently hidden. */
  override hidden: boolean;

  /** The currently selected wallpaper. */
  private currentSelected_: CurrentWallpaper|null;

  /** The index of the currently focused photo. */
  private focusedPhotoIndex_: number;

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

  /** The current personalization error state. */
  private error_: PersonalizationStateError|null;

  /** The singleton wallpaper provider interface. */
  private wallpaperProvider_: WallpaperProviderInterface =
      getWallpaperProvider();

  override connectedCallback() {
    super.connectedCallback();

    this.addEventListener('iron-resize', this.onResized_.bind(this));

    this.watch<GooglePhotosPhotosElement['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<GooglePhotosPhotosElement['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.watch<GooglePhotosPhotosElement['photos_']>(
        'photos_', state => state.wallpaper.googlePhotos.photos);
    this.watch<GooglePhotosPhotosElement['photosLoading_']>(
        'photosLoading_', state => state.wallpaper.loading.googlePhotos.photos);
    this.watch<GooglePhotosPhotosElement['photosResumeToken_']>(
        'photosResumeToken_',
        state => state.wallpaper.googlePhotos.resumeTokens.photos);
    this.watch<GooglePhotosPhotosElement['error_']>(
        'error_', state => state.error);
    this.updateFromStore();
  }

  /** Invoked on changes to |focusedPhotoIndex_|. */
  private onFocusedPhotoIndexChanged_(
      focusedPhotoIndex: GooglePhotosPhotosElement['focusedPhotoIndex_']) {
    // Attempt to focus the |element| at the focused index. Note that the
    // |element| may not be rendered as it could exist outside of the viewport.
    const selector = `.photo[photoindex="${focusedPhotoIndex}"]`;
    const element = this.$.grid.querySelector<HTMLElement>(selector);
    if (element) {
      element.focus();
      return;
    }

    // If the |element| was not rendered, it exists outside of the viewport. To
    // force it to render, focus the grid row which contains the |element| at
    // the focused index. Note that this will automatically trigger another call
    // to |onFocusedPhotoIndexChanged()|.
    this.photosByRow_.some((row, rowIndex) => {
      if (row.some(photo => photo.index === focusedPhotoIndex)) {
        this.$.grid.focusItem(rowIndex);
        return true;
      }
      return false;
    });
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

  /** Invoked on mouse down of a grid row. */
  private onGridRowMouseDown_(e: Event) {
    // Prevent the odd behavior of auto jumping to the focused photo when the
    // user clicks on any grid row.
    e.preventDefault();
  }

  /** Invoked on focus of a grid row. */
  private onGridRowFocused_() {
    // If |focusedPhotoIndex_| is -1, this is the first time focus has entered
    // the grid. In this case advance focus to the first photo.
    if (this.focusedPhotoIndex_ === -1) {
      this.focusedPhotoIndex_ = 0;
      return;
    }
    // When a grid row is focused, forward the focus event on to the photo at
    // the focused index.
    this.onFocusedPhotoIndexChanged_(this.focusedPhotoIndex_);
  }

  /** Invoked on key down of a grid row. */
  private onGridRowKeyDown_(e: KeyboardEvent&{
    model: {index: number, row: GooglePhotosPhotosRow},
  }) {
    let handled = false;

    switch (normalizeKeyForRTL(e.key, this.i18n('textdirection') === 'rtl')) {
      case 'ArrowDown':
        // To be consistent with default iron-list grid behavior, the down arrow
        // should only advance focus to the next grid row if a photo at the same
        // column index as is currently focused exists.
        if (e.model.index < this.photosByRow_.length - 1) {
          let colIndex = -1;
          e.model.row.some((photo, i) => {
            if (photo.index === this.focusedPhotoIndex_) {
              colIndex = i;
              return true;
            }
            return false;
          });
          assert(colIndex !== -1);
          const nextRow = this.photosByRow_[e.model.index + 1];
          if (colIndex < nextRow.length) {
            this.focusedPhotoIndex_ = nextRow[colIndex].index;
          }
        }
        handled = true;
        break;
      case 'ArrowLeft':
        this.focusedPhotoIndex_ = Math.max(this.focusedPhotoIndex_ - 1, 0);
        handled = true;
        break;
      case 'ArrowRight':
        this.focusedPhotoIndex_ =
            Math.min(this.focusedPhotoIndex_ + 1, this.photos_!.length - 1);
        handled = true;
        break;
      case 'ArrowUp':
        // To be consistent with default iron-list grid behavior, the up arrow
        // should only advance focus to the previous grid row if a photo at the
        // the same column index as is currently focused exists.
        if (e.model.index > 0) {
          let colIndex = -1;
          e.model.row.some((photo, i) => {
            if (photo.index === this.focusedPhotoIndex_) {
              colIndex = i;
              return true;
            }
            return false;
          });
          assert(colIndex !== -1);
          const previousRow = this.photosByRow_[e.model.index - 1];
          if (colIndex < previousRow.length) {
            this.focusedPhotoIndex_ = previousRow[colIndex].index;
          }
        }
        handled = true;
        break;
      case 'Tab':
        // The grid contains a single |focusable| row which becomes a focus trap
        // due to the synthetic redirect of focus events to photos. To escape
        // the trap, make the |focusable| row unfocusable until focus has
        // advanced to the next candidate.
        const focusable = this.$.grid.querySelector('[tabindex="0"]')!;
        focusable.setAttribute('tabindex', '-1');
        afterNextRender(this, () => focusable.setAttribute('tabindex', '0'));
        break;
    }

    if (handled) {
      e.preventDefault();
      e.stopPropagation();
    }
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_(hidden: GooglePhotosPhotosElement['hidden']) {
    if (hidden && this.error_ && this.error_.id === ERROR_ID) {
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

  /** Invoked on selection of a photo. `e.model.photo` is added by iron-list. */
  private onPhotoSelected_(e: WallpaperGridItemSelectedEvent&
                           {model: {photo: GooglePhotosPhoto}}) {
    assert(e.model.photo, 'google photos photos selected event has photo');
    if (!this.isPhotoPlaceholder_(e.model.photo)) {
      selectWallpaper(e.model.photo, this.wallpaperProvider_, this.getStore());
      recordWallpaperGooglePhotosSourceUMA(WallpaperGooglePhotosSource.PHOTOS);
    }
  }

  /** Invoked on changes to |photosBySection_|. */
  private onPhotosBySectionChanged_(
      photosBySection: GooglePhotosPhotosElement['photosBySection_']) {
    if (photosBySection === null) {
      // If the list of photos fails to load and is currently showing, display
      // an error to the user that allows them to make another attempt.
      if (!this.hidden) {
        this.dispatch(setErrorAction({
          id: ERROR_ID,
          message: this.i18n('googlePhotosError'),
          dismiss: {
            message: this.i18n('googlePhotosTryAgain'),
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
  private onPhotosPerRowChanged_(
      photosPerRow: GooglePhotosPhotosElement['photosPerRow_']) {
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
      photosResumeToken: GooglePhotosPhotosElement['photosResumeToken_']) {
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
      photos: GooglePhotosPhotosElement['photos_'],
      photosPerRow: GooglePhotosPhotosElement['photosPerRow_']):
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
      const date = mojoString16ToString(photo.date);

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
      photosBySection: GooglePhotosPhotosElement['photosBySection_']): string
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
      photosBySection: GooglePhotosPhotosElement['photosBySection_']): string
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

  /** Returns the number of photos or placeholders currently being displayed. */
  private getPhotosAriaSetSize_(): number {
    if (this.photos_) {
      return this.photos_.length;
    }

    return getPlaceholders().length * getNumberOfGridItemsPerRow();
  }

  /** Returns the aria label for the specified |photo|. */
  private getPhotoAriaLabel_(photo: GooglePhotosPhoto|null): string|undefined {
    if (photo) {
      return photo.id === PLACEHOLDER_ID ? this.i18n('ariaLabelLoading') :
                                           photo.name;
    }
    return undefined;
  }

  /** Returns the aria posinset index for the photo at index |i|. */
  private getPhotoAriaIndex_(i: number): number {
    return i + 1;
  }

  /** Returns whether the title for the specified grid |row| is visible. */
  private isGridRowTitleVisible_(
      row: GooglePhotosPhotosRow,
      photosBySection: GooglePhotosPhotosElement['photosBySection_']): boolean {
    return !!this.getGridRowDate_(row, photosBySection);
  }

  /** Returns whether the specified |photo| is a placeholder. */
  private isPhotoPlaceholder_(photo: GooglePhotosPhoto|null): boolean {
    return !!photo && photo.id === PLACEHOLDER_ID;
  }

  /** Returns whether the specified |photo| is currently selected. */
  private isPhotoSelected_(
      photo: GooglePhotosPhoto|null,
      currentSelected: GooglePhotosPhotosElement['currentSelected_'],
      pendingSelected: GooglePhotosPhotosElement['pendingSelected_']): boolean {
    if (!photo || (!currentSelected && !pendingSelected)) {
      return false;
    }
    // NOTE: Old clients may not support |dedupKey| when setting Google Photos
    // wallpaper, so use |id| in such cases for backwards compatibility.
    if (isGooglePhotosPhoto(pendingSelected) &&
        ((pendingSelected!.dedupKey &&
          isImageAMatchForKey(photo, pendingSelected!.dedupKey)) ||
         isImageAMatchForKey(photo, pendingSelected!.id))) {
      return true;
    }
    if (!pendingSelected && !!currentSelected &&
        (currentSelected.type === WallpaperType.kOnceGooglePhotos ||
         currentSelected.type === WallpaperType.kDailyGooglePhotos) &&
        isImageEqualToSelected(photo, currentSelected)) {
      return true;
    }
    return false;
  }
}

customElements.define(GooglePhotosPhotosElement.is, GooglePhotosPhotosElement);
