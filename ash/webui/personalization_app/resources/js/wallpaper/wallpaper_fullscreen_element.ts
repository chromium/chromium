// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays a transparent full screen
 * viewing mode of the currently selected wallpaper.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../common/icons.html.js';

import {FullscreenPreviewState} from 'chrome://resources/ash/common/personalization/wallpaper_state.js';
import {isNonEmptyFilePath} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';

import {CurrentWallpaper, WallpaperLayout} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {DisplayableImage} from './constants.js';
import {getWallpaperLayoutEnum, isGooglePhotosPhoto} from './utils.js';
import {setFullscreenStateAction} from './wallpaper_actions.js';
import {cancelPreviewWallpaper, confirmPreviewWallpaper, selectWallpaper} from './wallpaper_controller.js';
import {getTemplate} from './wallpaper_fullscreen_element.html.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const fullscreenClass = 'fullscreen-preview';
const fullscreenTransitionClass = 'fullscreen-preview-transition';

let shouldWaitForOpacityTransitions = true;

export function setShouldWaitForFullscreenOpacityTransitionsForTesting(
    value: boolean) {
  shouldWaitForOpacityTransitions = value;
}

// Wait for document.body opacity transition to end. Set a timeout for safety in
// case the opacity transition has been canceled.
async function waitForOpacityTransition(): Promise<void> {
  if (!shouldWaitForOpacityTransitions) {
    return;
  }
  const handler =
      await new Promise<(event: TransitionEvent) => void>(resolve => {
        const handler = (event: TransitionEvent) => {
          if (event.propertyName === 'opacity') {
            window.clearTimeout(timeoutId);
            resolve(handler);
          }
        };
        const timeoutId = window.setTimeout(() => resolve(handler), 250);
        document.body.addEventListener('transitionend', handler);
      });

  document.body.removeEventListener('transitionend', handler);
}

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
      fullscreenState_: {
        type: Object,
        observer: 'onFullscreenStateChanged_',
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
      selectedLayout_: {
        type: Number,
        value: null,
      },

      /**
       * Whether the full screen container and the corresponding controls (exit,
       * confirm, layout buttons) are shown.
       */
      showContainer_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private fullscreenState_: FullscreenPreviewState;
  private showLayoutOptions_: boolean;
  private currentSelected_: CurrentWallpaper|null;
  private pendingSelected_: DisplayableImage|null;
  private selectedLayout_: WallpaperLayout|null;
  private showContainer_: boolean;

  private onVisibilityChange_ = () => {
    if (document.visibilityState === 'hidden' &&
        this.fullscreenState_ !== FullscreenPreviewState.OFF) {
      // The user has probably switched to a different application. Cancel
      // preview immediately instead of waiting for fullscreenchange event.
      this.dispatch(setFullscreenStateAction(FullscreenPreviewState.OFF));
    }
  };

  private onPopState_ = () => {
    if (this.fullscreenState_ !== FullscreenPreviewState.OFF) {
      // The user has probably pressed the back button on an external mouse, or
      // swiped left to go back.
      this.dispatch(setFullscreenStateAction(FullscreenPreviewState.OFF));
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    this.$.container.addEventListener(
        'fullscreenchange', this.fullscreenChangeEventHander_.bind(this));

    this.watch<WallpaperFullscreenElement['fullscreenState_']>(
        'fullscreenState_', state => state.wallpaper.fullscreen);
    this.watch<WallpaperFullscreenElement['showLayoutOptions_']>(
        'showLayoutOptions_',
        state => !!state.wallpaper.pendingSelected &&
            (isNonEmptyFilePath(state.wallpaper.pendingSelected) ||
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

  private async onFullscreenStateChanged_(value: FullscreenPreviewState) {
    switch (value) {
      case FullscreenPreviewState.OFF:
        this.showContainer_ = false;
        document.body.classList.remove(fullscreenClass);
        await waitForOpacityTransition();
        document.body.classList.remove(fullscreenTransitionClass);
        this.selectedLayout_ = null;
        if (this.getFullscreenElement()) {
          await this.exitFullscreen();
        }
        cancelPreviewWallpaper(getWallpaperProvider());
        return;
      case FullscreenPreviewState.LOADING:
        // Do not assign this.showContainer_ here. If last state was
        // FullScreenPreviewState.OFF, showContainer_ should stay false. If last
        // state was FullScreenPreviewState.VISIBLE, showContainer_ should stay
        // true. This accounts for both entering full screen preview for the
        // first time, and for users clicking on the layout buttons.
        if (!this.getFullscreenElement()) {
          this.$.container.requestFullscreen();
        }
        return;
      case FullscreenPreviewState.VISIBLE:
        document.body.classList.add(fullscreenTransitionClass);
        document.body.classList.add(fullscreenClass);
        if (typeof this.selectedLayout_ !== 'number') {
          this.selectedLayout_ = WallpaperLayout.kCenterCropped;
        }
        await waitForOpacityTransition();
        this.showContainer_ = true;
        return;
    }
  }

  private fullscreenChangeEventHander_() {
    const hidden = !this.getFullscreenElement();
    if (hidden) {
      this.dispatch(setFullscreenStateAction(FullscreenPreviewState.OFF));
    } else {
      this.$.exit.focus();
    }
  }

  private async onClickExit_() {
    this.dispatch(setFullscreenStateAction(FullscreenPreviewState.OFF));
  }

  private async onClickConfirm_() {
    // Confirm the preview wallpaper before exiting fullscreen. In tablet
    // splitscreen, this prevents `WallpaperController::OnOverviewModeWillStart`
    // from triggering first, which leads to preview wallpaper getting canceled
    // before it gets confirmed (b/289133203).
    confirmPreviewWallpaper(getWallpaperProvider());
    this.dispatch(setFullscreenStateAction(FullscreenPreviewState.OFF));
  }

  private async onClickLayout_(event: MouseEvent) {
    assert(
        isNonEmptyFilePath(this.pendingSelected_) ||
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
