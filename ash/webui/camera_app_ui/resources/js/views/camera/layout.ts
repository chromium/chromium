// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {cssStyle} from '../../css.js';
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

  private readonly viewportRule = cssStyle('#preview-viewport');

  private readonly contentRule = cssStyle('.preview-content');

  constructor(private readonly cameraManager: CameraManager) {}

  private setContentSize(width: number, height: number) {
    this.contentRule.setProperty('width', `${width}px`);
    this.contentRule.setProperty('height', `${height}px`);
    this.faceOverlay.width = width;
    this.faceOverlay.height = height;
  }

  private setViewportSize(width: number, height: number) {
    this.viewportRule.setProperty('width', `${width}px`);
    this.viewportRule.setProperty('height', `${height}px`);
  }

  /**
   * Sets the offset between video content and viewport.
   */
  private setContentOffset(dx: number, dy: number) {
    this.contentRule.setProperty('left', `${dx}px`);
    this.contentRule.setProperty('top', `${dy}px`);
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

    if (this.cameraManager.useSquareResolution()) {
      const viewportSize = Math.min(boxW, boxH);
      this.setViewportSize(viewportSize, viewportSize);
      const scale =
          viewportSize / Math.min(video.videoHeight, video.videoWidth);
      const contentW = scale * video.videoWidth;
      const contentH = scale * video.videoHeight;
      this.setContentSize(contentW, contentH);
      this.setContentOffset(
          (viewportSize - contentW) / 2, (viewportSize - contentH) / 2);
    } else {
      const scale = Math.min(boxH / video.videoHeight, boxW / video.videoWidth);
      const contentW = scale * video.videoWidth;
      const contentH = scale * video.videoHeight;
      this.setViewportSize(contentW, contentH);
      this.setContentSize(contentW, contentH);
      this.setContentOffset(0, 0);
    }
  }
}
