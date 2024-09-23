// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that previews the current selected
 * wallpaper.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../common/icons.html.js';

import {assert} from 'chrome://resources/js/assert.js';

import {CurrentAttribution, CurrentWallpaper, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getWallpaperAriaLabel, getWallpaperSrc} from './utils.js';
import {getTemplate} from './wallpaper_preview_element.html.js';

export class WallpaperPreviewElement extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-preview';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      attribution_: {
        type: Object,
        value: null,
      },
      image_: {
        type: Object,
        value: null,
      },
      imageLoading_: Boolean,
      loading_: {
        type: Boolean,
        computed: 'computeLoading_(imageLoading_, image_)',
      },
      policyControlled_: {
        type: Boolean,
        computed: 'isPolicyControlled_(image_)',
      },
    };
  }

  private attribution_: CurrentAttribution|null;
  private image_: CurrentWallpaper|null;
  private imageLoading_: boolean;
  private loading_: boolean;
  private policyControlled_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.watch('attribution_', state => state.wallpaper.attribution);
    this.watch('image_', state => state.wallpaper.currentSelected);
    this.watch(
        'imageLoading_',
        state => state.wallpaper.loading.setImage > 0 ||
            state.wallpaper.loading.selected.image ||
            state.wallpaper.loading.selected.attribution ||
            state.wallpaper.loading.refreshWallpaper);
    this.updateFromStore();
  }

  /**
   * Navigate to wallpaper collections page.
   */
  private onClickWallpaper_() {
    assert(!!this.image_ && this.image_.type !== WallpaperType.kPolicy);
    PersonalizationRouterElement.instance().goToRoute(Paths.COLLECTIONS);
  }

  private getWallpaperSrc_(image: CurrentWallpaper|null): string|null {
    return getWallpaperSrc(image);
  }

  private getImageAltDescription_(
      image: CurrentWallpaper|null,
      attribution: CurrentAttribution|null): string {
    return getWallpaperAriaLabel(
        image, attribution, /*dailyRefreshState=*/ null);
  }

  private computeLoading_(): boolean {
    return this.imageLoading_ || !this.image_;
  }

  private isPolicyControlled_(): boolean {
    return !!this.image_ && this.image_.type === WallpaperType.kPolicy;
  }
}

customElements.define(WallpaperPreviewElement.is, WallpaperPreviewElement);
