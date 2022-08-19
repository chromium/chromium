// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays a single grid item.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../css/common.css.js';

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
      imageSrc: {
        type: String,
        observer: 'onImageSrcChanged_',
      },

      index: Number,
      primaryText: String,
      secondaryText: String,

      selected: {
        type: Boolean,
        observer: 'onSelectedChanged_',
      },

      placeholder: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  /** The source for the image to render for the grid item. */
  imageSrc: string|undefined;

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

  /**
   * Whether to show a loading animation instead of the real image. Defaults to
   * false. Will also set a placeholder html attribute.
   */
  placeholder: boolean;

  // Invoked on changes to |imageSrc|.
  private onImageSrcChanged_(imageSrc: WallpaperGridItem['imageSrc']) {
    // Hide the |image| element until it has successfully loaded. Note that it
    // is intentional that the |image| element remain hidden on failure.
    this.$.image.setAttribute('hidden', '');
    this.$.image.onload = (imageSrc && imageSrc.length) ?
        () => this.$.image.removeAttribute('hidden') :
        null;
  }

  private onSelectedChanged_(selected: boolean|undefined) {
    if (typeof selected === 'boolean') {
      this.setAttribute('aria-selected', selected.toString());
    } else {
      this.removeAttribute('aria-selected');
    }
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
