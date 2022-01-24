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

import {assert} from 'chrome://resources/js/assert.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getWallpaperProvider} from './mojo_interface_provider.js';
import {setFullscreenEnabledAction} from './personalization_actions.js';
import {WallpaperLayout} from './personalization_app.mojom-webui.js';
import {cancelPreviewWallpaper, confirmPreviewWallpaper, selectWallpaper} from './personalization_controller.js';
import {DisplayableImage} from './personalization_reducers.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {getWallpaperLayoutEnum} from './utils.js';

const fullscreenClass = 'fullscreen-preview';

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
       * Note that this contains information about the non-preview wallpaper
       * that was set before entering fullscreen mode.
       * @type {!CurrentWallpaper}
       * @private
       */
      currentSelected_: {
        type: Object,
        value: null,
      },
      /**
       * This will be set during the duration of preview mode.
       * @type {?DisplayableImage}
       * @private
       */
      pendingSelected_: {
        type: Object,
        value: null,
      },
      /**
       * When preview mode starts, this is set to currentSelected.layout. If the
       * user changes layout option, this will be updated locally to track which
       * option the user has selected (currentSelected.layout does not change
       * until confirmPreviewWallpaper is called).
       * @type {?WallpaperLayout}
       * @private
       */
      selectedLayout_: {
        type: Number,
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
        state => state.pendingSelected?.hasOwnProperty('path'));
    this.watch('currentSelected_', state => state.currentSelected);
    this.watch('pendingSelected_', state => state.pendingSelected);
    // Visibility change will fire in case of alt+tab, closing the window, or
    // anything else that exits out of full screen mode.
    window.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'hidden' &&
          !!this.getFullscreenElement()) {
        this.exitFullscreen();
        // Cancel preview immediately instead of waiting for fullscreenchange
        // event.
        cancelPreviewWallpaper(this.wallpaperProvider_);
      }
    });
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
    if (value && !this.getFullscreenElement()) {
      // Reset to default wallpaper layout each time.
      this.selectedLayout_ = WallpaperLayout.kCenterCropped;
      this.shadowRoot.getElementById('container')
          .requestFullscreen()
          .then(() => document.body.classList.add(fullscreenClass));
    } else if (!value && this.getFullscreenElement()) {
      this.selectedLayout_ = null;
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
      this.dispatch(setFullscreenEnabledAction(/*enabled=*/ false));
      document.body.classList.remove(fullscreenClass);
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
  async onClickLayout_(event) {
    assert(this.pendingSelected_?.hasOwnProperty('path'));
    const layout = getWallpaperLayoutEnum(event.currentTarget.dataset.layout);
    await selectWallpaper(
        /** @type {!DisplayableImage} */ (this.pendingSelected_),
        this.wallpaperProvider_, this.getStore(), layout);
    this.selectedLayout_ = layout;
  }

  /**
   * @param {?WallpaperLayout} selectedLayout
   * @param {string} str
   * @return {string}
   * @private
   */
  getLayoutAriaSelected_(selectedLayout, str) {
    assert(str === 'FILL' || str === 'CENTER');
    const layout = getWallpaperLayoutEnum(str);
    return (selectedLayout === layout).toString();
  }
}

customElements.define(WallpaperFullscreen.is, WallpaperFullscreen);
