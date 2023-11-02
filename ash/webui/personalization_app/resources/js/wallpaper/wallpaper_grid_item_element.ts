// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays a single grid item.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../css/common.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getLoadingPlaceholderAnimationDelay} from './utils.js';
import {getTemplate} from './wallpaper_grid_item_element.html.js';

const enum ImageStatus {
  LOADING = 'loading',
  ERROR = 'error',
  READY = 'ready',
}

function getDataIndex(event: Event&{currentTarget: HTMLImageElement}): number {
  const dataIndex = event.currentTarget.dataset['index'];
  assert(typeof dataIndex === 'string', 'data-index property required');
  const index = parseInt(dataIndex, 10);
  assert(!isNaN(index), `could not parseInt on ${dataIndex}`);
  return index;
}

export class WallpaperGridItem extends PolymerElement {
  static get is(): 'wallpaper-grid-item' {
    return 'wallpaper-grid-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      src: {
        type: Object,
        observer: 'onImageSrcChanged_',
      },

      index: Number,
      primaryText: String,
      secondaryText: String,

      isGooglePhotos: {
        type: Boolean,
        value: false,
      },

      selected: {
        type: Boolean,
        observer: 'onSelectedChanged_',
      },

      imageStatus_: {
        type: Array,
        value() {
          return [];
        },
        observer: 'onImageStatusChanged_',
      },
    };
  }

  /**
   * The source for the image to render for the grid item. Will display a
   * placeholder loading animation if src is undefined.
   * If |src| is an array, will display the first two images side by side.
   */
  src: Url|Url[]|undefined;

  /** The index of the grid item within its parent grid. */
  index: number;

  /** The primary text to render for the grid item. */
  primaryText: string|undefined;

  /** The secondary text to render for the grid item. */
  secondaryText: string|undefined;

  /**
   * Passed to cr-auto-img to send google photos auth token on image request.
   */
  isGooglePhotos: boolean;

  /**
   * Whether the grid item is currently selected. Controls the aria-selected
   * html attribute. When undefined, aria-selected will be removed.
   */
  selected: boolean|undefined;

  // Track if images are loaded, failed, or ready to display.
  private imageStatus_: ImageStatus[];

  // Invoked on changes to |imageSrc|.
  private onImageSrcChanged_(
      src: Url|Url[]|undefined, old: Url|Url[]|undefined) {
    // Set loading status if src has just changed while we wait for new images.
    const oldSrcArray = this.getSrcArray_(old);
    this.imageStatus_ = this.getSrcArray_(src).map(({url}, i) => {
      if (oldSrcArray.length > i && oldSrcArray[i].url === url) {
        // If the underlying url has not changed, keep the prior image status.
        // If we have a new |Url| object but the underlying url is the same, the
        // img onload event will not fire and reset the status to ready.
        return this.imageStatus_[i];
      }
      return ImageStatus.LOADING;
    });
  }

  private onSelectedChanged_(selected: boolean|undefined) {
    if (typeof selected === 'boolean') {
      this.setAttribute('aria-selected', selected.toString());
    } else {
      this.removeAttribute('aria-selected');
    }
  }

  private onImageStatusChanged_(imageStatus: ImageStatus[]) {
    if (imageStatus.length === 0 ||
        (imageStatus.includes(ImageStatus.LOADING) &&
         !imageStatus.includes(ImageStatus.ERROR))) {
      this.setAttribute('placeholder', '');
    } else {
      this.removeAttribute('placeholder');
    }
  }

  private onImgError_(event: Event&{currentTarget: HTMLImageElement}) {
    const targetIndex = getDataIndex(event);
    this.imageStatus_ = this.imageStatus_.map(
        (status, index) => index === targetIndex ? ImageStatus.ERROR : status);
  }

  private onImgLoad_(event: Event&{currentTarget: HTMLImageElement}) {
    const targetIndex = getDataIndex(event);
    this.imageStatus_ = this.imageStatus_.map(
        (status, index) => index === targetIndex ? ImageStatus.READY : status);
  }

  private getSrcArray_(src: Url|Url[]|undefined): Url[] {
    if (!src) {
      return [];
    }
    if (Array.isArray(src)) {
      return src.slice(0, 2);
    }
    return [src];
  }

  private isImageHidden_(imageStatus: ImageStatus[]): boolean {
    // |imageStatus| is usually a non-empty array when this function is called.
    // But there are weird cases where dom-repeat will still call this function
    // when |src| goes from an array back to undefined.
    assert(Array.isArray(imageStatus), 'image status must be an array');
    // Do not show the image while loading because it has an ugly white frame.
    // Do not show the image on error either because it has an ugly broken red
    // icon symbol.
    // Wait until all images are ready to show any of them.
    return imageStatus.length === 0 ||
        imageStatus.some(status => status !== ImageStatus.READY);
  }

  /** Returns the delay to use for the grid item's placeholder animation. */
  private getItemPlaceholderAnimationDelay_(index: WallpaperGridItem['index']):
      string {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  /** Whether the primary text is currently visible. */
  private isPrimaryTextVisible_() {
    return !!this.primaryText && !!this.primaryText.length;
  }

  /** Whether the secondary text is currently visible. */
  private isSecondaryTextVisible_() {
    return !!this.secondaryText && !!this.secondaryText.length;
  }

  /** Whether any text is currently visible. */
  private isTextVisible_() {
    return this.isSecondaryTextVisible_() || this.isPrimaryTextVisible_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [WallpaperGridItem.is]: WallpaperGridItem;
  }
}

customElements.define(WallpaperGridItem.is, WallpaperGridItem);
