// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * import {Resolution} from '../type.js';
 */
var Resolution = Resolution || {};

/**
 * import {assert} from 'chrome://resources/js/assert.js';
 */
var assert = assert || {};

/**
 * Creates a controller to handle layouts of Camera view.
 */
cca.views.camera.Layout = class {
  /**
   * @public
   */
  constructor() {
    /**
     * CSS style of the viewport in square mode.
     * @type {!CSSStyleDeclaration}
     * @private
     */
    this.squareViewport_ =
        this.constructor.cssStyle_('body.square-preview #preview-wrapper');

    /**
     * CSS style of the video in square mode.
     * @type {!CSSStyleDeclaration}
     * @private
     */
    this.squareVideo_ =
        this.constructor.cssStyle_('body.square-preview .preview-content');

    /**
     * CSS style of what is currently put as camera preview.
     * @type {!CSSStyleDeclaration}
     * @private
     */
    this.previewContent_ = this.constructor.cssStyle_('.preview-content');
  }

  /**
   * Gets the CSS style by the given selector.
   * @param {string} selector Selector text.
   * @return {!CSSStyleDeclaration}
   * @private
   */
  static cssStyle_(selector) {
    const rule = cca.views.camera.Layout.cssRules_.find(
        (rule) => rule.selectorText === selector);
    assert(rule !== undefined);
    assert(rule.style !== null);
    return rule.style;
  }

  /**
   * Updates the video element size for previewing in the window.
   * @return {!Resolution} Letterbox size.
   * @private
   */
  updatePreviewSize_() {
    // Make video content keeps its aspect ratio inside the window's
    // inner-bounds; it may fill up the window or be letterboxed when
    // fullscreen/maximized. Don't use app-window.innerBounds' width/height
    // properties during resizing as they are not updated immediately.
    const video = document.querySelector('#preview-video');
    let contentWidth = 0;
    let contentHeight = 0;
    if (video.videoHeight) {
      const scale = cca.state.get('square-mode') ?
          Math.min(window.innerHeight, window.innerWidth) /
              Math.min(video.videoHeight, video.videoWidth) :
          Math.min(
              window.innerHeight / video.videoHeight,
              window.innerWidth / video.videoWidth);
      contentWidth = scale * video.videoWidth;
      contentHeight = scale * video.videoHeight;
      this.previewContent_.setProperty('width', `${contentWidth}px`);
      this.previewContent_.setProperty('height', `${contentHeight}px`);
    }
    let viewportW = contentWidth;
    let viewportH = contentHeight;
    cca.state.set('square-preview', cca.state.get('square-mode'));
    if (cca.state.get('square-mode')) {
      viewportW = viewportH = Math.min(contentWidth, contentHeight);
      this.squareVideo_.setProperty(
          'left', `${(viewportW - contentWidth) / 2}px`);
      this.squareVideo_.setProperty(
          'top', `${(viewportH - contentHeight) / 2}px`);
      this.squareViewport_.setProperty('width', `${viewportW}px`);
      this.squareViewport_.setProperty('height', `${viewportH}px`);
    }
    return new Resolution(
        window.innerWidth - viewportW, window.innerHeight - viewportH);
  }

  /**
   * Updates the layout for video-size or window-size changes.
   */
  update() {
    const fullWindow = cca.util.isWindowFullSize();
    const tall = window.innerHeight > window.innerWidth;
    const tabletLandscape = fullWindow && !tall;
    cca.state.set('tablet-landscape', tabletLandscape);
    cca.state.set('max-wnd', fullWindow);
    cca.state.set('tall', tall);

    const {width: letterboxW, height: letterboxH} = this.updatePreviewSize_();
    const isLetterboxW = letterboxH < letterboxW;

    cca.state.set('w-letterbox', isLetterboxW);
    if (isLetterboxW) {
      const modeWidth =
          document.querySelector('#modes-group').getBoundingClientRect().width;
      let layoutToggled = false;
      [[modeWidth + 30, 'w-letterbox-s'],
       [modeWidth + 30 + 72, 'w-letterbox-m'],
       [(modeWidth + 30) * 2, 'w-letterbox-l'],
       [Infinity, 'w-letterbox-xl'],
      ]
          .forEach(
              ([wSize, classname]) => cca.state.set(
                  classname,
                  /* Enable only state which the letterboxW size falls in range
                   * of its wSize and previous wSize. And disable all other
                   * states. */
                  !layoutToggled && (layoutToggled = letterboxW <= wSize)));
    } else {
      // preview-vertical-dock: Dock bottom line of preview between gallery and
      //                        mode selector.
      // otherwise: Vertically center the preview.
      cca.state.set('preview-vertical-dock', letterboxH / 2 >= 112);
    }
  }
};

/**
 * CSS rules.
 * @type {!Array<!CSSRule>}
 * @private
 */
cca.views.camera.Layout.cssRules_ =
    [].slice.call(document.styleSheets[0].cssRules);
