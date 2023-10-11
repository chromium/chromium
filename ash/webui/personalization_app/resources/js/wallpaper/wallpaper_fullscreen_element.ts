// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays a transparent full screen
 * viewing mode of the currently selected wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../common/icons.html.js';

import {assert} from 'chrome://resources/js/assert.js';

import {CurrentWallpaper, WallpaperLayout} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {DisplayableImage} from './constants.js';
import {getWallpaperLayoutEnum, isFilePath, isGooglePhotosPhoto} from './utils.js';
import {setFullscreenEnabledAction} from './wallpaper_actions.js';
import {cancelPreviewWallpaper, confirmPreviewWallpaper, selectWallpaper} from './wallpaper_controller.js';
import {getTemplate} from './wallpaper_fullscreen_element.html.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const fullscreenClass = 'fullscreen-preview';

export interface WallpaperFullscreenElement {
  $: {container: HTMLDivElement, exit: HTMLElement};
}

export class WallpaperFullscreenElement extends WithPersonalizationStore {
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

  private onVisibilityChange_ = () => {
    if (document.visibilityState === 'hidden' && this.visible_) {
      // Cancel preview immediately instead of waiting for fullscreenchange
      // event.
      cancelPreviewWallpaper(getWallpaperProvider());
    }
  };

  private onPopState_ = () => {
    if (this.visible_) {
      cancelPreviewWallpaper(getWallpaperProvider());
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    this.$.container.addEventListener(
        'fullscreenchange', this.onFullscreenChange_.bind(this));

    this.watch<WallpaperFullscreenElement['visible_']>(
        'visible_', state => state.wallpaper.fullscreen);
    this.watch<WallpaperFullscreenElement['showLayoutOptions_']>(
        'showLayoutOptions_',
        state => !!state.wallpaper.pendingSelected &&
            (isFilePath(state.wallpaper.pendingSelected) ||
             isGooglePhotosPhoto(state.wallpaper.pendingSelected)));
    this.watch<WallpaperFullscreenElement['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<WallpaperFullscreenElement['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);

    // Visibility change will fire in case of alt+tab, closing the window, or
    // anything else that exits out of full screen mode.
    window.addEventListener('visibilitychange', this.onVisibilityChange_);
    window.addEventListener('popstate', this.onPopState_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('visibilitychange', this.onVisibilityChange_);
    window.removeEventListener('popstate', this.onPopState_);
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
      cancelPreviewWallpaper(getWallpaperProvider());
      this.dispatch(setFullscreenEnabledAction(/*enabled=*/ false));
      document.body.classList.remove(fullscreenClass);
    } else {
      this.$.exit.focus();
    }
  }

  private async onClickExit_() {
    await cancelPreviewWallpaper(getWallpaperProvider());
    await this.exitFullscreen();
  }

  private async onClickConfirm_() {
    // Confirm the preview wallpaper before exiting fullscreen. In tablet
    // splitscreen, this prevents `WallpaperController::OnOverviewModeWillStart`
    // from triggering first, which leads to preview wallpaper getting canceled
    // before it gets confirmed (b/289133203).
    await confirmPreviewWallpaper(getWallpaperProvider());
    await this.exitFullscreen();
  }

  private async onClickLayout_(event: MouseEvent) {
    assert(
        isFilePath(this.pendingSelected_) ||
            isGooglePhotosPhoto(this.pendingSelected_),
        'pendingSelected must be a local image or a Google Photos image to set layout');
    const layout = getWallpaperLayoutEnum(
        (event.currentTarget as HTMLButtonElement).dataset['layout']!);
    await selectWallpaper(
        this.pendingSelected_, getWallpaperProvider(), this.getStore(), layout);
    this.selectedLayout_ = layout;
  }

  private getLayoutAriaPressed_(
      selectedLayout: WallpaperLayout, str: 'FILL'|'CENTER'): string {
    assert(str === 'FILL' || str === 'CENTER');
    const layout = getWallpaperLayoutEnum(str);
    return (selectedLayout === layout).toString();
  }
}

customElements.define(
    WallpaperFullscreenElement.is, WallpaperFullscreenElement);
