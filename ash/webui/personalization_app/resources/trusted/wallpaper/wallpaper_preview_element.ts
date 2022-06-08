// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that previews the current selected
 * wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../common/icons.html.js';
import '../../common/common_style.css.js';
import './trusted_style.css.js';
import '../cros_button_style.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {getLocalStorageAttribution, isNonEmptyArray} from '../../common/utils.js';
import {CurrentWallpaper, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {hasHttpScheme, removeHighResolutionSuffix} from '../utils.js';

import {getWallpaperProvider} from './wallpaper_interface_provider.js';
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
      imageLoading_: {
        type: Boolean,
      },
    };
  }

  private image_: CurrentWallpaper|null;
  private imageLoading_: boolean;
  private wallpaperProvider_: WallpaperProviderInterface;

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  override connectedCallback() {
    super.connectedCallback();
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

  /**
   * Return a chrome://image or data:// url to load the image safely. Returns
   * empty string in case |image| is null or invalid.
   */
  private getImageSrc_(image: CurrentWallpaper|null): string {
    if (image && image.url) {
      if (hasHttpScheme(image.url.url)) {
        return `chrome://image?${removeHighResolutionSuffix(image.url.url)}`;
      }
      return image.url.url;
    }
    return '';
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
