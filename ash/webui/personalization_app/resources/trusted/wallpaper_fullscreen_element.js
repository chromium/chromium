// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays a transparent full screen
 * viewing mode of the currently selected wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../common/icons.js';
import {assert} from 'chrome://resources/js/assert.m.js'
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getWallpaperLayoutEnum} from '../common/utils.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {setFullscreenEnabledAction} from './personalization_actions.js';
import {cancelPreviewWallpaper, confirmPreviewWallpaper, setCustomWallpaperLayout} from './personalization_controller.js';
import {updateDailyRefreshWallpaper} from './personalization_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';

/** @polymer */
export class WallpaperFullscreen extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-fullscreen';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      visible_: {
        type: Boolean,
        value: false,
        observer: 'onVisibleChanged_',
      },
      /** @private */
      showLayoutOptions_: {
        type: Boolean,
        value: false,
      },
      /**
       * TODO(b/202392508) remove this debug view when transparency works
       * @type {?chromeos.personalizationApp.mojom.CurrentWallpaper}
       * @private
       */
      image_: {
        type: Object,
        value: null,
      },
    };
  }

  /** @override */
  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.shadowRoot.getElementById('container')
        .addEventListener(
            'fullscreenchange', this.onFullscreenChange_.bind(this));
    this.watch('visible_', state => state.fullscreen);
    this.watch(
        'showLayoutOptions_',
        state => state.currentSelected?.type ===
            chromeos.personalizationApp.mojom.WallpaperType.kCustomized);
    this.watch(
        'showDailyRefresh_',
        state => state.currentSelected?.type ===
                chromeos.personalizationApp.mojom.WallpaperType.kDaily &&
            state.dailyRefresh.collectionId && !state.pendingSelected);
    this.watch('showConfirm_', state => !!state.pendingSelected);
    this.watch('image_', state => state.currentSelected);
  }

  /**
   * Wrapper function to mock out for testing.
   * @return {?Element}
   */
  getFullscreenElement() {
    return document.fullscreenElement;
  }

  /** Wrapper function to mock out for testing.  */
  exitFullscreen() {
    document.exitFullscreen();
  }

  /**
   * @param {boolean} value
   * @private
   */
  onVisibleChanged_(value) {
    if (value) {
      // Should only reach here if there is a valid image selected.
      assert(!!this.getState().currentSelected);
    }
    if (value && !this.getFullscreenElement()) {
      this.shadowRoot.getElementById('container').requestFullscreen();
    } else if (!value && this.getFullscreenElement()) {
      this.exitFullscreen();
    }
  }

  /** @private */
  onFullscreenChange_() {
    const hidden = !this.getFullscreenElement();
    this.shadowRoot.getElementById('container').hidden = hidden;
    if (hidden) {
      // SWA also supports exiting fullscreen when users press ESC. In this
      // case, the preview mode may be still on so we have to call cancel
      // preview.
      // This call is no-op when the user clicks on exit button or set as
      // wallpaper button.
      cancelPreviewWallpaper(this.wallpaperProvider_);
      this.dispatch(setFullscreenEnabledAction(/*enabled=*/false));
    }
  }

  /** @private */
  onClickExit_() {
    cancelPreviewWallpaper(this.wallpaperProvider_);
    this.exitFullscreen();
  }

  /** @private */
  onClickConfirm_() {
    confirmPreviewWallpaper(this.wallpaperProvider_);
    this.exitFullscreen();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onClickLayout_(event) {
    const layout = getWallpaperLayoutEnum(event.currentTarget.dataset.layout);
    setCustomWallpaperLayout(layout, this.wallpaperProvider_, this.getStore());
  }

  /**
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @param {string} str
   * @return {string}
   * @private
   */
  getLayoutAriaSelected_(image, str) {
    assert(str === 'FILL' || str === 'CENTER');
    const layout = getWallpaperLayoutEnum(str);
    return (image?.layout === layout).toString();
  }
}

customElements.define(WallpaperFullscreen.is, WallpaperFullscreen);
