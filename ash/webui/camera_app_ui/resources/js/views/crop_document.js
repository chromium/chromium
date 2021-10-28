// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '../chrome_util.js';
import * as dom from '../dom.js';
import {Box, Line, Point, Size, Vector, vectorFromPoints} from '../geometry.js';
import {I18nString} from '../i18n_string.js';
import {Rotation, ViewName} from '../type.js';
import * as util from '../util.js';

import {Option, Options, Review} from './review.js';

/**
 * The closest distance ratio with respect to corner space size. The dragging
 * corner should not be closer to the 3 lines formed by another 3 corners than
 * this ratio times scale of corner space size.
 */
const CLOSEST_DISTANCE_RATIO = 1 / 10;

const ROTATIONS = [
  Rotation.ANGLE_0,
  Rotation.ANGLE_90,
  Rotation.ANGLE_180,
  Rotation.ANGLE_270,
];

/**
 * @typedef {{
 *   el: !HTMLButtonElement,
 *   pt: !Point,
 *   pointerId: ?number,
 * }}
 */
let Corner;  // eslint-disable-line no-unused-vars

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
    this.imageFrame_ = dom.getFrom(this.root, '.review-frame', HTMLDivElement);

    /**
     * Size of image frame.
     * @type {?Size}
     * @private
     */
    this.frameSize_ = new Size(0, 0);

    /**
     * The original size of the image to be cropped.
     * @type {?Size}
     * @private
     */
    this.imageOriginalSize_ = null;

    /**
     * Space size coordinates in |this.corners_|. Will change when image client
     * area resized.
     * @type {?Size}
     * @private
     */
    this.cornerSpaceSize_ = null;

    /**
     * @const {!SVGElement}
     * @private
     */
    this.cropAreaContainer_ =
        dom.getFrom(this.root, '.crop-area-container', SVGElement);

    /**
     * @const {!SVGPolygonElement}
     * @private
     */
    this.cropArea_ = dom.getFrom(this.image_, '.crop-area', SVGPolygonElement);

    /**
     * Index of |ROTATION| as current photo rotation.
     * @type {number}
     * @private
     */
    this.rotation_ = 0;

    /**
     * @type {?Array<!Point>}
     * @private
     */
    this.initialCorners_ = null;

    /**
     * Coordinates of document with respect to |this.cornerSpaceSize_|.
     * @type {!Array<!Corner>}
     * @private
     */
    this.corners_ = (() => {
      const ret = [];
      for (let i = 0; i < 4; i++) {
        const tpl = util.instantiateTemplate('#document-drag-point-template');
        ret.push({
          el: dom.getFrom(tpl, `.dot`, HTMLButtonElement),
          pt: new Point(0, 0),
          pointerId: null,
        });
        this.image_.appendChild(tpl);
      }
      return ret;
    })();

    const clockwiseBtn = dom.getFrom(
        this.root, 'button[i18n-aria=rotate_clockwise_button]',
        HTMLButtonElement);
    clockwiseBtn.addEventListener('click', () => {
      this.rotation_ = (this.rotation_ + 1) % ROTATIONS.length;
      this.updateImage_();
    });

    const counterclockwiseBtn = dom.getFrom(
        this.root, 'button[i18n-aria=rotate_counterclockwise_button]',
        HTMLButtonElement);
    counterclockwiseBtn.addEventListener('click', () => {
      this.rotation_ =
          (this.rotation_ + ROTATIONS.length - 1) % ROTATIONS.length;
      this.updateImage_();
    });

    const cornerSize = (() => {
      const style = this.corners_[0].el.computedStyleMap();
      const width = util.getStyleValueInPx(style, 'width');
      const height = util.getStyleValueInPx(style, 'height');
      return new Size(width, height);
    })();

    this.corners_.forEach((corn) => {
      // Start dragging on one corner.
      corn.el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        assert(e.target === corn.el);
        this.setDragging_(corn, assertInstanceof(e, PointerEvent).pointerId);
      });

      // Use arrow key to move corner.
      const KEYS = ['ArrowUp', 'ArrowLeft', 'ArrowDown', 'ArrowRight'];
      const getKeyIndex = (e) =>
          KEYS.indexOf(assertInstanceof(e, KeyboardEvent).key);
      const KEY_MOVEMENTS = [
        new Vector(0, -1),
        new Vector(-1, 0),
        new Vector(0, 1),
        new Vector(1, 0),
      ];
      /**
       * Maps from key index in |KEYS| to corresponding movement handler.
       * @type {!Map<number, function()>}
       */
      const keyHandlers = new Map();
      let keyInterval = null;
      const clearKeydown = () => {
        if (keyInterval !== null) {
          keyInterval.stop();
          keyInterval = null;
        }
        keyHandlers.clear();
      };

      corn.el.addEventListener('blur', (e) => {
        clearKeydown();
      });

      corn.el.addEventListener('keydown', (e) => {
        const keyIdx = getKeyIndex(e);
        if (keyIdx === -1 || keyHandlers.has(keyIdx)) {
          return;
        }
        const movement = KEY_MOVEMENTS[(keyIdx + this.rotation_) % 4];
        const move = () => {
          const {x: curX, y: curY} = corn.pt;
          const nextPt = new Point(curX + movement.x, curY + movement.y);
          const validPt = this.mapToValidArea_(corn, nextPt);
          if (validPt === null) {
            return;
          }
          corn.pt = validPt;
          this.updateCornerEl_();
        };
        move();
        keyHandlers.set(keyIdx, move);

        if (keyInterval === null) {
          const PRESS_TIMEOUT = 500;
          const HOLD_INTERVAL = 100;
          keyInterval = new util.DelayInterval(() => {
            keyHandlers.forEach((handler) => handler());
          }, PRESS_TIMEOUT, HOLD_INTERVAL);
        }
      });

      corn.el.addEventListener('keyup', (e) => {
        const keyIdx = getKeyIndex(e);
        if (keyIdx === -1) {
          return;
        }
        keyHandlers.delete(keyIdx);
        if (keyHandlers.size === 0) {
          clearKeydown();
        }
      });
    });

    // Stop dragging.
    for (const eventName of ['pointerup', 'pointerleave', 'pointercancel']) {
      this.image_.addEventListener(eventName, (e) => {
        e.preventDefault();
        this.clearDragging_(assertInstanceof(e, PointerEvent).pointerId);
      });
    }

    // Move drag corner.
    this.image_.addEventListener('pointermove', (e) => {
      e.preventDefault();

      const pointerId = assertInstanceof(e, PointerEvent).pointerId;
      const corn = this.findDragging_(pointerId);
      if (corn === null) {
        return;
      }
      assert(corn.el.classList.contains('dragging'));

      let dragX = e.offsetX;
      let dragY = e.offsetY;
      const target = assertInstanceof(e.target, HTMLElement);
      // The offsetX, offsetY of corners.el are measured from their own left,
      // top.
      if (this.corners_.find(({el}) => el === target) !== undefined) {
        const style = target.attributeStyleMap;
        dragX += util.getStyleValueInPx(style, 'left') - cornerSize.width / 2;
        dragY += util.getStyleValueInPx(style, 'top') - cornerSize.height / 2;
      }

      const validPt = this.mapToValidArea_(corn, new Point(dragX, dragY));
      if (validPt === null) {
        return;
      }
      corn.pt = validPt;
      this.updateCornerEl_();
    });

    // Prevent contextmenu popup triggered by long touch.
    this.image_.addEventListener('contextmenu', (e) => {
      if (e['pointerType'] === 'touch') {
        e.preventDefault();
      }
    });
  }

  /**
   * @param {!Array<!Point>} corners Initial guess from corner detector.
   * @return {!Promise<{corners: !Array<!Point>, rotation: !Rotation}>} Returns
   *     new selected corners to be cropped and its rotation.
   */
  async reviewCropArea(corners) {
    this.initialCorners_ = corners;
    this.cornerSpaceSize_ = null;
    await super.startReview({
      positive: new Options(
          new Option(I18nString.LABEL_CROP_DONE, {exitValue: true}),
          ),
      negative: new Options(),
    });
    const newCorners = this.corners_.map(
        ({pt: {x, y}}) => new Point(
            x / this.cornerSpaceSize_.width, y / this.cornerSpaceSize_.height));
    return {corners: newCorners, rotation: ROTATIONS[this.rotation_]};
  }

  /**
   * @param {!Corner} corn
   * @param {number} pointerId
   * @private
   */
  setDragging_(corn, pointerId) {
    corn.el.classList.add('dragging');
    corn.pointerId = pointerId;
  }

  /**
   * @param {number} pointerId
   * @return {?Corner}
   */
  findDragging_(pointerId) {
    return this.corners_.find(({pointerId: id}) => id === pointerId) || null;
  }

  /**
   * @param {number} pointerId
   * @private
   */
  clearDragging_(pointerId) {
    const corn = this.findDragging_(pointerId);
    if (corn === null) {
      return;
    }
    corn.el.classList.remove('dragging');
    corn.pointerId = null;
  }

  /**
   * @param {!Corner} corn
   * @param {!Point} pt
   * @return {?Point}
   * @private
   */
  mapToValidArea_(corn, pt) {
    pt = new Point(
        Math.max(Math.min(pt.x, this.cornerSpaceSize_.width), 0),
        Math.max(Math.min(pt.y, this.cornerSpaceSize_.height), 0));

    const idx = this.corners_.findIndex((c) => c === corn);
    assert(idx !== -1);
    const prevPt = this.corners_[(idx + 3) % 4].pt;
    const nextPt = this.corners_[(idx + 1) % 4].pt;
    const restPt = this.corners_[(idx + 2) % 4].pt;
    const closestDist =
        Math.min(this.cornerSpaceSize_.width, this.cornerSpaceSize_.height) *
        CLOSEST_DISTANCE_RATIO;
    const prevDir = vectorFromPoints(restPt, prevPt).direction();
    const prevBorder = (new Line(prevPt, prevDir)).moveParallel(closestDist);
    const nextDir = vectorFromPoints(nextPt, restPt).direction();
    const nextBorder = (new Line(nextPt, nextDir)).moveParallel(closestDist);
    const restDir = vectorFromPoints(nextPt, prevPt).direction();
    const restBorder = (new Line(prevPt, restDir)).moveParallel(closestDist);

    if (prevBorder.isInward(pt) && nextBorder.isInward(pt) &&
        restBorder.isInward(pt)) {
      return pt;
    }

    const prevBorderPt = prevBorder.intersect(restBorder);
    if (prevBorderPt === null) {
      // May completely overlapped.
      return null;
    }
    const nextBorderPt = nextBorder.intersect(restBorder);
    if (nextBorderPt === null) {
      // May completely overlapped.
      return null;
    }
    const box = new Box(assertInstanceof(this.cornerSpaceSize_, Size));

    // Find boundary points of valid area by cases of whether |prevBorderPt| and
    // |nextBorderPt| are inside/outside the box.
    const boundaryPts = [];
    if (!box.inside(prevBorderPt) && !box.inside(nextBorderPt)) {
      const intersectPts = box.segmentIntersect(prevBorderPt, nextBorderPt);
      if (intersectPts.length === 0) {
        // Valid area is completely outside the bounding box.
        return null;
      } else {
        boundaryPts.push(...intersectPts);
      }
    } else {
      if (box.inside(prevBorderPt)) {
        const boxPt =
            box.rayIntersect(prevBorderPt, prevBorder.direction.reverse());
        boundaryPts.push(boxPt, prevBorderPt);
      } else {
        const newPrevBorderPt = box.rayIntersect(
            nextBorderPt, vectorFromPoints(prevBorderPt, nextBorderPt));
        boundaryPts.push(newPrevBorderPt);
      }

      if (box.inside(nextBorderPt)) {
        const boxPt = box.rayIntersect(nextBorderPt, nextBorder.direction);
        boundaryPts.push(nextBorderPt, boxPt);
      } else {
        const newBorderPt = box.rayIntersect(
            prevBorderPt, vectorFromPoints(nextBorderPt, prevBorderPt));
        boundaryPts.push(newBorderPt);
      }
    }

    /**
     * @param {!Point} pt1
     * @param {!Point} pt2
     * @param {!Point} pt3
     * @return {{dist2: number, nearest: !Point}} Square distance of |pt3| to
     *     segment formed by |pt1| and |pt2| and the corresponding nearest
     *     point on the segment.
     */
    const distToSegment = (pt1, pt2, pt3) => {
      // Minimum Distance between a Point and a Line:
      // http://paulbourke.net/geometry/pointlineplane/
      const v12 = vectorFromPoints(pt2, pt1);
      const v13 = vectorFromPoints(pt3, pt1);
      const u = (v12.x * v13.x + v12.y * v13.y) / v12.length2();
      if (u <= 0) {
        return {dist2: v13.length2(), nearest: pt1};
      }
      if (u >= 1) {
        return {dist2: vectorFromPoints(pt3, pt2).length2(), nearest: pt2};
      }
      const projection = vectorFromPoints(pt1).add(v12.multiply(u)).point();
      return {
        dist2: vectorFromPoints(projection, pt3).length2(),
        nearest: projection,
      };
    };

    // Project |pt| to nearest point on boundary.
    let mn = Infinity;
    let mnPt = null;
    for (let i = 1; i < boundaryPts.length; i++) {
      const {dist2, nearest} =
          distToSegment(boundaryPts[i - 1], boundaryPts[i], pt);
      if (dist2 < mn) {
        mn = dist2;
        mnPt = nearest;
      }
    }
    return assertInstanceof(mnPt, Point);
  }

  /**
   * @private
   */
  updateCornerEl_() {
    const cords = this.corners_.map(({pt: {x, y}}) => `${x},${y}`).join(' ');
    this.cropArea_.setAttribute('points', cords);
    this.corners_.forEach((corn) => {
      const style = corn.el.attributeStyleMap;
      style.set('left', CSS.px(corn.pt.x));
      style.set('top', CSS.px(corn.pt.y));
    });
  }

  /**
   * Updates image position/size with respect to |this.rotation_|,
   * |this.frameSize_| and |this.imageOriginalSize_|.
   * @private
   */
  updateImage_() {
    const {width: frameW, height: frameH} = this.frameSize_;
    const {width: rawImageW, height: rawImageH} = this.imageOriginalSize_;
    const style = this.image_.attributeStyleMap;

    let rotatedW = rawImageW;
    let rotatedH = rawImageH;
    if (ROTATIONS[this.rotation_] === Rotation.ANGLE_90 ||
        ROTATIONS[this.rotation_] === Rotation.ANGLE_270) {
      [rotatedW, rotatedH] = [rotatedH, rotatedW];
    }
    const scale = Math.min(1, frameW / rotatedW, frameH / rotatedH);
    const newImageW = scale * rawImageW;
    const newImageH = scale * rawImageH;
    style.set('width', CSS.px(newImageW));
    style.set('height', CSS.px(newImageH));
    this.cropAreaContainer_.setAttribute(
        'viewBox', `0 0 ${newImageW} ${newImageH}`);

    // Update corner space.
    if (this.cornerSpaceSize_ === null) {
      this.initialCorners_.forEach(({x, y}, idx) => {
        this.corners_[idx].pt = new Point(x * newImageW, y * newImageH);
      });
      this.initialCorners_ = null;
    } else {
      const oldImageW = this.cornerSpaceSize_?.width || newImageW;
      const oldImageH = this.cornerSpaceSize_?.height || newImageH;
      this.corners_.forEach((corn) => {
        corn.pt = new Point(
            corn.pt.x / oldImageW * newImageW,
            corn.pt.y / oldImageH * newImageH);
      });
    }
    this.cornerSpaceSize_ = new Size(newImageW, newImageH);

    const originX =
        frameW / 2 + rotatedW * scale / 2 * [-1, 1, 1, -1][this.rotation_];
    const originY =
        frameH / 2 + rotatedH * scale / 2 * [-1, -1, 1, 1][this.rotation_];
    style.set('left', CSS.px(originX));
    style.set('top', CSS.px(originY));

    const deg = ROTATIONS[this.rotation_];
    style.set(
        'transform', new CSSTransformValue([new CSSRotate(CSS.deg(deg))]));

    this.updateCornerEl_();
  }

  /**
   * @override
   */
  async setReviewPhoto(blob) {
    const image = new Image();
    await this.loadImage_(image, blob);
    this.imageOriginalSize_ = new Size(image.width, image.height);
    const style = this.image_.attributeStyleMap;
    if (style.has('background-image')) {
      const oldUrl = style.get('background-image')
                         .toString()
                         .match(/url\(['"]([^'"]+)['"]\)/)[1];
      URL.revokeObjectURL(oldUrl);
    }
    style.set('background-image', `url('${image.src}')`);

    this.rotation_ = 0;
  }

  /**
   * @override
   */
  layout() {
    super.layout();

    const rect = this.imageFrame_.getBoundingClientRect();
    this.frameSize_ = new Size(rect.width, rect.height);
    this.updateImage_();
    // Clear all dragging corners.
    for (const corn of this.corners_) {
      if (corn.pointerId !== null) {
        this.clearDragging_(corn.pointerId);
      }
    }
  }
}
