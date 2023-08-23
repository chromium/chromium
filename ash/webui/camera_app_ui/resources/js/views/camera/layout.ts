// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CameraManager} from '../../device/index.js';
import * as dom from '../../dom.js';
import * as state from '../../state.js';
import {windowController} from '../../window_controller.js';

/**
 * Creates a controller to handle layouts of Camera view.
 */
export class Layout {
  private readonly previewBox = dom.get('#preview-box', HTMLDivElement);

  private readonly faceOverlay =
      dom.get('#preview-face-overlay', HTMLCanvasElement);

  private readonly rootStyle = document.documentElement.style;

  constructor(private readonly cameraManager: CameraManager) {}

  private setContentSize(width: number, height: number) {
    // Not using attributeStyleMap / StylePropertyMap here since custom
    // properties can only use CSSUnparsedValue, which doesn't make the code
    // simpler. (@property / CSS.registerProperty only applies when the var is
    // computed, but doesn't affect the type when the var is set, See
    // https://drafts.css-houdini.org/css-properties-values-api/#parsing-custom-properties)
    this.rootStyle.setProperty(
        '--preview-content-width', CSS.px(width).toString());
    this.rootStyle.setProperty(
        '--preview-content-height', CSS.px(height).toString());
    this.faceOverlay.width = width;
    this.faceOverlay.height = height;
  }

  private setViewportSize(width: number, height: number) {
    this.rootStyle.setProperty(
        '--preview-viewport-width', CSS.px(width).toString());
    this.rootStyle.setProperty(
        '--preview-viewport-height', CSS.px(height).toString());
  }

  /**
   * Sets the offset between video content and viewport.
   */
  private setContentOffset(dx: number, dy: number) {
    this.rootStyle.setProperty('--preview-content-left', CSS.px(dx).toString());
    this.rootStyle.setProperty('--preview-content-top', CSS.px(dy).toString());
  }

  /**
   * Updates the layout for video-size or window-size changes.
   */
  update(): void {
    const fullWindow = windowController.isFullscreenOrMaximized();
    const tall = window.innerHeight > window.innerWidth;
    state.set(state.State.TABLET_LANDSCAPE, fullWindow && !tall);
    state.set(state.State.MAX_WND, fullWindow);
    state.set(state.State.TALL, tall);

    const {width: boxW, height: boxH} = this.previewBox.getBoundingClientRect();
    const video = dom.get('#preview-video', HTMLVideoElement);

    // When the app is minimized, the width and height of the video will be
    // zero. Don't update the layout for such case.
    const {videoWidth, videoHeight} = video;
    if (videoWidth === 0 || videoHeight === 0) {
      return;
    }

    if (this.cameraManager.useSquareResolution()) {
      const viewportSize = Math.min(boxW, boxH);
      this.setViewportSize(viewportSize, viewportSize);
      const scale = viewportSize / Math.min(videoHeight, videoWidth);
      const contentW = scale * videoWidth;
      const contentH = scale * videoHeight;
      this.setContentSize(contentW, contentH);
      this.setContentOffset(
          (viewportSize - contentW) / 2, (viewportSize - contentH) / 2);
    } else {
      const scale = Math.min(boxH / videoHeight, boxW / videoWidth);
      const contentW = scale * videoWidth;
      const contentH = scale * videoHeight;
      this.setViewportSize(contentW, contentH);
      this.setContentSize(contentW, contentH);
      this.setContentOffset(0, 0);
    }
  }
}
