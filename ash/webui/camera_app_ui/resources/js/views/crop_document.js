// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../chrome_util.js';
import * as dom from '../dom.js';
import {Point} from '../geometry.js';
import {I18nString} from '../i18n_string.js';
import {Resolution, ViewName} from '../type.js';
import * as util from '../util.js';

import {Option, Options, Review} from './review.js';

/**
 * View controller for review document crop area page.
 */
export class CropDocument extends Review {
  /**
   * @public
   */
  constructor() {
    super(ViewName.CROP_DOCUMENT);

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.cropContainer_ =
        dom.getFrom(this.root, '#crop-container', HTMLDivElement);

    /**
     * @type {?Resolution}
     * @private
     */
    this.cropContainerSize_ = null;

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.cropArea_ = dom.getFrom(this.root, '.crop-area', HTMLDivElement);

    /**
     * @type {!Array<!Point>}
     * @private
     */
    this.initialCorners_ = [];

    /**
     * Coordinates of document with respect to |cropContainerSize_|.
     * @type {!Array<!Point>}
     * @private
     */
    this.corners_ = [];

    /**
     * @type {!Array<!HTMLDivElement>}
     * @private
     */
    this.cornerEls_ = (() => {
      const ret = [];
      for (let i = 0; i < 4; i++) {
        const tpl = util.instantiateTemplate('#document-drag-point-template');
        ret.push(dom.getFrom(tpl, `.dot`, HTMLDivElement));
        this.cropContainer_.appendChild(tpl);
      }
      return ret;
    })();
    const cornerSize = (() => {
      const style =
          dom.get('#crop-container .dot', HTMLDivElement).computedStyleMap();
      const width = util.getStyleValueInPx(style, 'width');
      const height = util.getStyleValueInPx(style, 'height');
      return new Resolution(width, height);
    })();

    // TODO(b/203181872): Tweak the drag behavior.

    /**
     * @type {?HTMLDivElement}
     */
    let dragging = null;

    // Start dragging on one corner.
    this.cornerEls_.forEach((el) => {
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        if (e.target !== el) {
          return;
        }
        dragging = el;
      });
    });

    // Stop dragging.
    for (const eventName of ['pointerup', 'pointerleave', 'pointercancel']) {
      this.cropContainer_.addEventListener(eventName, (e) => {
        e.preventDefault();
        dragging = null;
      });
    }

    // Move drag corner.
    this.cropContainer_.addEventListener('pointermove', (e) => {
      e.preventDefault();
      if (dragging === null) {
        return;
      }

      this.corners_.forEach((corn, idx) => {
        const el = this.cornerEls_[idx];
        if (el !== dragging) {
          return;
        }

        let dragX = e.offsetX;
        let dragY = e.offsetY;
        const target = assertInstanceof(e.target, HTMLElement);
        if (this.cornerEls_.includes(/** @type {!HTMLDivElement} */ (target))) {
          // The offsetX, offsetY of cornerEls are measured from their own left,
          // top.
          const style = target.attributeStyleMap;
          dragX += util.getStyleValueInPx(style, 'left') - cornerSize.width / 2;
          dragY += util.getStyleValueInPx(style, 'top') - cornerSize.height / 2;
        }
        dragX = Math.max(Math.min(dragX, this.cropContainerSize_.width), 0);
        dragY = Math.max(Math.min(dragY, this.cropContainerSize_.height), 0);
        const dragPt = new Point(dragX, dragY);
        this.corners_[idx] = dragPt;
      });

      this.updateCorners_();
    });

    // Prevent contextmenu popup triggered by long touch.
    this.cropContainer_.addEventListener('contextmenu', (e) => {
      e.preventDefault();
    });
  }

  /**
   * @param {!Array<!Point>} corners Initial guess from corner detector.
   * @return {!Promise<!Array<!Point>>} Return new selected corners to be
   *     cropped.
   */
  async reviewCropArea(corners) {
    this.initialCorners_ = corners;
    this.corners_ = [];
    this.cropContainerSize_ = null;
    await super.startReview({
      positive: new Options(
          new Option(I18nString.LABEL_CROP_DONE, {exitValue: true}),
          ),
      negative: new Options(),
    });
    return this.corners_.map(
        ({x, y}) => new Point(
            x / this.cropContainerSize_.width,
            y / this.cropContainerSize_.height));
  }

  /**
   * @private
   */
  updateCorners_() {
    const cords = this.corners_.map(({x, y}) => `${x}px ${y}px`).join(',');
    this.cropArea_.attributeStyleMap.set('clip-path', `polygon(${cords})`);
    this.corners_.forEach(({x, y}, idx) => {
      const style = this.cornerEls_[idx].attributeStyleMap;
      style.set('left', CSS.px(x));
      style.set('top', CSS.px(y));
    });
  }

  /**
   * @override
   */
  layout() {
    super.layout();

    const {offsetWidth: width, offsetHeight: height, offsetLeft, offsetTop} =
        this.image_;
    const style = this.cropContainer_.attributeStyleMap;
    style.set('left', CSS.px(offsetLeft));
    style.set('top', CSS.px(offsetTop));
    style.set('width', CSS.px(width));
    style.set('height', CSS.px(height));
    if (this.cropContainerSize_ === null) {
      this.corners_ = this.initialCorners_.map(
          ({x, y}) => new Point(x * width, y * height));
    } else {
      this.corners_ = this.corners_.map(
          ({x, y}) => new Point(
              x / this.cropContainerSize_.width * width,
              y / this.cropContainerSize_.height * height));
    }
    this.cropContainerSize_ = new Resolution(width, height);
    this.updateCorners_();
  }
}
