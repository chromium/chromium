// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperErrorElement displays an error svg when wallpaper
 * collections fail to load. This is done in-line instead of using
 * iron-iconset-svg because iron-iconset-svg is designed for small square icons
 * that may have multiple sizes, not large rectangular svgs.
 */

import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './wallpaper_error_element.html.js';

const WallpaperErrorBase = I18nMixin(PolymerElement);

export class WallpaperErrorElement extends WallpaperErrorBase {
  static get is() {
    return 'wallpaper-error';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(WallpaperErrorElement.is, WallpaperErrorElement);
