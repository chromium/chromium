// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays a single grid item.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../css/common.css.js';

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getLoadingPlaceholderAnimationDelay} from './utils.js';
import {getTemplate} from './wallpaper_grid_item_element.html.js';

export interface WallpaperGridItem {
  $: {image: HTMLImageElement};
}

export class WallpaperGridItem extends PolymerElement {
  static get is() {
    return 'wallpaper-grid-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      src: {
        type: Url,
        observer: 'onImageSrcChanged_',
      },

      index: Number,
      primaryText: String,
      secondaryText: String,

      selected: {
        type: Boolean,
        observer: 'onSelectedChanged_',
      },

      loading_: {
        type: Boolean,
        value: true,
        observer: 'onLoadingChanged_',
      },

      error_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * The source for the image to render for the grid item. Will display a
   * placeholder loading animation if src is undefined.
   */
  src: Url|undefined;

  /** The index of the grid item within its parent grid. */
  index: number;

  /** The primary text to render for the grid item. */
  primaryText: string|undefined;

  /** The secondary text to render for the grid item. */
  secondaryText: string|undefined;

  /**
   * Whether the grid item is currently selected. Controls the aria-selected
   * html attribute. When undefined, aria-selected will be removed.
   */
  selected: boolean|undefined;

  // Received a new image that has not been downloaded yet for display.
  private loading_: boolean;

  // Image failed to download.
  private error_: boolean;

  // Invoked on changes to |imageSrc|.
  private onImageSrcChanged_(_: WallpaperGridItem['src']) {
    // Set loading status if src has just changed while we wait for new image.
    this.setProperties({
      loading_: true,
      error_: false,
    });
  }

  private onSelectedChanged_(selected: boolean|undefined) {
    if (typeof selected === 'boolean') {
      this.setAttribute('aria-selected', selected.toString());
    } else {
      this.removeAttribute('aria-selected');
    }
  }

  private onLoadingChanged_(loading: boolean) {
    if (loading) {
      this.setAttribute('placeholder', '');
    } else {
      this.removeAttribute('placeholder');
    }
  }

  private onImgError_() {
    this.setProperties({loading_: false, error_: true});
  }

  private onImgLoad_() {
    this.setProperties({loading_: false, error_: false});
  }

  private isImageHidden_(loading: boolean, error: boolean) {
    // Do not show the image while loading because it has an ugly white frame.
    // Do not show the image on error either because it has an ugly broken red
    // icon symbol.
    return loading || error;
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

customElements.define(WallpaperGridItem.is, WallpaperGridItem);
