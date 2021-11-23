// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays a single grid item.
 */

import '/common/styles.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class WallpaperGridItemElement extends PolymerElement {
  static get is() {
    return 'wallpaper-grid-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(WallpaperGridItemElement.is, WallpaperGridItemElement);
