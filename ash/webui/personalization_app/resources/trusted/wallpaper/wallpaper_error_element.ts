// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperErrorElement displays an error svg when wallpaper
 * collections fail to load. This is done in-line instead of using
 * iron-iconset-svg because iron-iconset-svg is designed for small square icons
 * that may have multiple sizes, not large rectangular svgs.
 */

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const WithI18n: {new (): PolymerElement&I18nBehavior} =
    mixinBehaviors([I18nBehavior], PolymerElement);

export class WallpaperError extends WithI18n {
  static get is() {
    return 'wallpaper-error';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(WallpaperError.is, WallpaperError);
