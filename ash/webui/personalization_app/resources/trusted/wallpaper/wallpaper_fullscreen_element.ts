// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays a transparent full screen
 * viewing mode of the currently selected wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../common/icons.html.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {DisplayableImage} from '../../common/constants.js';
import {CurrentWallpaper, WallpaperLayout, WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getWallpaperLayoutEnum, isFilePath, isGooglePhotosPhoto} from '../utils.js';

import {setFullscreenEnabledAction} from './wallpaper_actions.js';
import {cancelPreviewWallpaper, confirmPreviewWallpaper, selectWallpaper} from './wallpaper_controller.js';
import {getTemplate} from './wallpaper_fullscreen_element.html.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const fullscreenClass = 'fullscreen-preview';

export interface WallpaperFullscreen {
  $: {container: HTMLDivElement, exit: HTMLElement};
}

export class WallpaperFullscreen extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-fullscreen';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      visible_: {
        type: Boolean,
        observer: 'onVisibleChanged_',
      },
      showLayoutOptions_: Boolean,
      /**
       * Note that this contains information about the non-preview wallpaper
       * that was set before entering fullscreen mode.
       */
      currentSelected_: Object,
      /** This will be set during the duration of preview mode. */
      pendingSelected_: Object,
      /**
       * When preview mode starts, this is set to the default layout. If the
       * user changes layout option, this will be updated locally to track which
       * option the user has selected (currentSelected.layout does not change
       * until confirmPreviewWallpaper is called).
       */
      selectedLayout_: Number,
    };
  }

  private visible_: boolean = false;
  private showLayoutOptions_: boolean = false;
  private currentSelected_: CurrentWallpaper|null = null;
  private pendingSelected_: DisplayableImage|null = null;
  private selectedLayout_: WallpaperLayout|null = null;

  private wallpaperProvider_: WallpaperProviderInterface;

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  /** Add override when tsc is updated to 4.3+. */
  override connectedCallback() {
    super.connectedCallback();
    this.$.container.addEventListener(
        'fullscreenchange', this.onFullscreenChange_.bind(this));

    this.watch<WallpaperFullscreen['visible_']>(
        'visible_', state => state.wallpaper.fullscreen);
    this.watch<WallpaperFullscreen['showLayoutOptions_']>(
        'showLayoutOptions_',
        state => !!state.wallpaper.pendingSelected &&
            (isFilePath(state.wallpaper.pendingSelected) ||
             isGooglePhotosPhoto(state.wallpaper.pendingSelected)));
    this.watch<WallpaperFullscreen['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<WallpaperFullscreen['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);

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

  /** Wrapper function to mock out for testing. */
  getFullscreenElement(): Element|null {
    return document.fullscreenElement;
  }

  /** Wrapper function to mock out for testing.  */
  exitFullscreen(): Promise<void> {
    return document.exitFullscreen();
  }

  private onVisibleChanged_(value: boolean) {
    if (value && !this.getFullscreenElement()) {
      // Reset to default wallpaper layout each time.
      this.selectedLayout_ = WallpaperLayout.kCenterCropped;
      this.$.container.requestFullscreen().then(
          () => document.body.classList.add(fullscreenClass));
    } else if (!value && this.getFullscreenElement()) {
      this.selectedLayout_ = null;
      this.exitFullscreen();
    }
  }

  private onFullscreenChange_() {
    const hidden = !this.getFullscreenElement();
    this.$.container.hidden = hidden;
    if (hidden) {
      // SWA also supports exiting fullscreen when users press ESC. In this
      // case, the preview mode may be still on so we have to call cancel
      // preview. This call is no-op when the user clicks on set as wallpaper
      // button.
      cancelPreviewWallpaper(this.wallpaperProvider_);
      this.dispatch(setFullscreenEnabledAction(/*enabled=*/ false));
      document.body.classList.remove(fullscreenClass);
    } else {
      this.$.exit.focus();
    }
  }

  private async onClickExit_() {
    await this.exitFullscreen();
    await cancelPreviewWallpaper(this.wallpaperProvider_);
  }

  private async onClickConfirm_() {
    // Begin to exit fullscreen mode before confirming preview wallpaper. This
    // makes local images and online images execute updates in the same order.
    await this.exitFullscreen();
    await confirmPreviewWallpaper(this.wallpaperProvider_);
  }

  private async onClickLayout_(event: MouseEvent) {
    assert(
        isFilePath(this.pendingSelected_) ||
            isGooglePhotosPhoto(this.pendingSelected_),
        'pendingSelected must be a local image or a Google Photos image to set layout');
    const layout = getWallpaperLayoutEnum(
        (event.currentTarget as HTMLButtonElement).dataset['layout']!);
    await selectWallpaper(
        this.pendingSelected_, this.wallpaperProvider_, this.getStore(),
        layout);
    this.selectedLayout_ = layout;
  }

  private getLayoutAriaPressed_(
      selectedLayout: WallpaperLayout, str: 'FILL'|'CENTER'): string {
    assert(str === 'FILL' || str === 'CENTER');
    const layout = getWallpaperLayoutEnum(str);
    return (selectedLayout === layout).toString();
  }
}

customElements.define(WallpaperFullscreen.is, WallpaperFullscreen);
