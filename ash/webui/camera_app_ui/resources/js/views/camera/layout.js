// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {cssStyle} from '../../css.js';
import * as dom from '../../dom.js';
import * as state from '../../state.js';
import {Mode} from '../../type.js';
import {windowController} from '../../window_controller.js';

/**
 * Creates a controller to handle layouts of Camera view.
 */
export class Layout {
  /**
   * @public
   */
  constructor() {
    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.previewBox_ = dom.get('#preview-box', HTMLDivElement);

    /**
     * @const {!HTMLCanvasElement}
     * @private
     */
    this.faceOverlay_ = dom.get('#preview-face-overlay', HTMLCanvasElement);

    /**
     * @const {!CSSStyleDeclaration}
     * @private
     */
    this.viewportRule_ = cssStyle('#preview-viewport');

    /**
     * @const {!CSSStyleDeclaration}
     * @private
     */
    this.contentRule_ = cssStyle('.preview-content');
  }

  /**
   * @param {number} width
   * @param {number} height
   * @private
   */
  setContentSize_(width, height) {
    this.contentRule_.setProperty('width', `${width}px`);
    this.contentRule_.setProperty('height', `${height}px`);
    this.faceOverlay_.width = width;
    this.faceOverlay_.height = height;
  }

  /**
   * @param {number} width
   * @param {number} height
   * @private
   */
  setViewportSize_(width, height) {
    this.viewportRule_.setProperty('width', `${width}px`);
    this.viewportRule_.setProperty('height', `${height}px`);
  }

  /**
   * Sets the offset between video content and viewport.
   * @param {number} dx
   * @param {number} dy
   * @private
   */
  setContentOffset_(dx, dy) {
    this.contentRule_.setProperty('left', `${dx}px`);
    this.contentRule_.setProperty('top', `${dy}px`);
  }

  /**
   * Updates the layout for video-size or window-size changes.
   */
  update() {
    const fullWindow = windowController.isFullscreenOrMaximized();
    const tall = window.innerHeight > window.innerWidth;
    state.set(state.State.TABLET_LANDSCAPE, fullWindow && !tall);
    state.set(state.State.MAX_WND, fullWindow);
    state.set(state.State.TALL, tall);

    const {width: boxW, height: boxH} =
        this.previewBox_.getBoundingClientRect();
    const video = dom.get('#preview-video', HTMLVideoElement);

    if (state.get(Mode.SQUARE)) {
      const viewportSize = Math.min(boxW, boxH);
      this.setViewportSize_(viewportSize, viewportSize);
      const scale =
          viewportSize / Math.min(video.videoHeight, video.videoWidth);
      const contentW = scale * video.videoWidth;
      const contentH = scale * video.videoHeight;
      this.setContentSize_(contentW, contentH);
      this.setContentOffset_(
          (viewportSize - contentW) / 2, (viewportSize - contentH) / 2);
    } else {
      const scale = Math.min(boxH / video.videoHeight, boxW / video.videoWidth);
      const contentW = scale * video.videoWidth;
      const contentH = scale * video.videoHeight;
      this.setViewportSize_(contentW, contentH);
      this.setContentSize_(contentW, contentH);
      this.setContentOffset_(0, 0);
    }
  }
}
