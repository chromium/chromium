// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays a single grid item.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../common/styles.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class WallpaperGridItem extends PolymerElement {
  static get is() {
    return 'wallpaper-grid-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      imageSrc: String,
      primaryText: String,
      secondaryText: String,

      selected: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** The source for the image to render for the grid item. */
  imageSrc: string|undefined;

  /** The primary text to render for the grid item. */
  primaryText: string|undefined;

  /** The secondary text to render for the grid item. */
  secondaryText: string|undefined;

  /** Whether the grid item is currently selected. */
  selected: boolean;

  /** Whether the image is currently visible. */
  private isImageVisible_() {
    return !!this.imageSrc?.length;
  }

  /** Whether the primary text is currently visible. */
  private isPrimaryTextVisible_() {
    return !!this.primaryText?.length;
  }

  /** Whether the secondary text is currently visible. */
  private isSecondaryTextVisible_() {
    return !!this.secondaryText?.length;
  }

  /** Whether any text is currently visible. */
  private isTextVisible_() {
    return this.isSecondaryTextVisible_() || this.isPrimaryTextVisible_();
  }
}

customElements.define(WallpaperGridItem.is, WallpaperGridItem);
