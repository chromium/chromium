// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that previews the current selected
 * wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../common/icons.html.js';
import '../../css/common.css.js';
import '../../css/wallpaper.css.js';
import '../../css/cros_button_style.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {CurrentWallpaper, WallpaperType} from '../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isNonEmptyArray} from '../utils.js';

import {getLocalStorageAttribution, getWallpaperSrc} from './utils.js';
import {WallpaperObserver} from './wallpaper_observer.js';
import {getTemplate} from './wallpaper_preview_element.html.js';

export class WallpaperPreview extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-preview';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      image_: {
        type: Object,
        value: null,
      },
      imageLoading_: Boolean,
    };
  }

  private image_: CurrentWallpaper|null;
  private imageLoading_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    WallpaperObserver.initWallpaperObserverIfNeeded();
    this.watch('image_', state => state.wallpaper.currentSelected);
    this.watch(
        'imageLoading_',
        state => state.wallpaper.loading.setImage > 0 ||
            state.wallpaper.loading.selected ||
            state.wallpaper.loading.refreshWallpaper);
    this.updateFromStore();
  }

  /**
   * Navigate to wallpaper collections page.
   */
  private onClickWallpaper_() {
    assert(!!this.image_ && this.image_.type !== WallpaperType.kPolicy);
    PersonalizationRouter.instance().goToRoute(Paths.COLLECTIONS);
  }

  private getWallpaperSrc_(image: CurrentWallpaper|null): string|null {
    return getWallpaperSrc(image);
  }

  private getImageAltDescription_(image: CurrentWallpaper|null): string {
    if (!image) {
      return `${this.i18n('currentlySet')} ${
          this.i18n('unknownImageAttribution')}`;
    }
    if (image.type === WallpaperType.kDefault) {
      return `${this.i18n('currentlySet')} ${this.i18n('defaultWallpaper')}`;
    }
    if (isNonEmptyArray(image.attribution)) {
      return [this.i18n('currentlySet'), ...image.attribution].join(' ');
    }
    // Fallback to cached attribution.
    const attribution = getLocalStorageAttribution(image.key);
    if (isNonEmptyArray(attribution)) {
      return [this.i18n('currentlySet'), ...attribution].join(' ');
    }
    return `${this.i18n('currentlySet')} ${
        this.i18n('unknownImageAttribution')}`;
  }

  /**
   * Returns visible state of loading placeholder.
   */
  private showPlaceholders_(
      imageLoading: boolean, image: CurrentWallpaper|null): boolean {
    return imageLoading || !image;
  }

  private isPolicyControlled_(image: CurrentWallpaper|null): boolean {
    return !!image && image.type === WallpaperType.kPolicy;
  }
}

customElements.define(WallpaperPreview.is, WallpaperPreview);
