// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '../assert.js';
import * as dom from '../dom.js';
import {Box, Line, Point, Size, Vector, vectorFromPoints} from '../geometry.js';
import {I18nString} from '../i18n_string.js';
import {speak} from '../toast.js';
import {Rotation, ViewName} from '../type.js';
import * as util from '../util.js';

import {Option, Options, Review} from './review.js';

/**
 * Delay for movement announcer gathering user pressed key to announce first
 * feedback in milliseconds.
 */
const MOVEMENT_ANNOUNCER_START_DELAY_MS = 500;

/**
 * Interval for movement announcer to announce next movement.
 */
const MOVEMENT_ANNOUNCER_INTERVAL_MS = 2000;

/**
 * Maps from sign of x, y movement to corresponding i18n labels to be announced.
 */
const MOVEMENT_ANNOUNCE_LABELS = new Map([
  [
    -1,
    new Map<number, I18nString>([
      [-1, I18nString.MOVING_IN_TOP_LEFT_DIRECTION],
      [0, I18nString.MOVING_IN_LEFT_DIRECTION],
      [1, I18nString.MOVING_IN_BOTTOM_LEFT_DIRECTION],
    ]),
  ],
  [
    0,
    new Map<number, I18nString>([
      [-1, I18nString.MOVING_IN_TOP_DIRECTION],
      [1, I18nString.MOVING_IN_BOTTOM_DIRECTION],
    ]),
  ],
  [
    1,
    new Map<number, I18nString>([
      [-1, I18nString.MOVING_IN_TOP_RIGHT_DIRECTION],
      [0, I18nString.MOVING_IN_RIGHT_DIRECTION],
      [1, I18nString.MOVING_IN_BOTTOM_RIGHT_DIRECTION],
    ]),
  ],
]);

/**
 * Announces the movement direction of document corner with screen reader.
 */
class MovementAnnouncer {
  /**
   * Interval to throttle the consecutive announcement.
   */
  private announceInterval: util.DelayInterval|null = null;

  /**
   * X component of last not announced movement.
   */
  private lastXMovement = 0;

  /**
   * Y component of last not announced movement.
   */
  private lastYMovement = 0;

  updateMovement(dx: number, dy: number) {
    this.lastXMovement = dx;
    this.lastYMovement = dy;
    if (this.announceInterval === null) {
      this.announceInterval = new util.DelayInterval(() => {
        this.announce();
      }, MOVEMENT_ANNOUNCER_START_DELAY_MS, MOVEMENT_ANNOUNCER_INTERVAL_MS);
    }
  }

  private announce() {
    if (this.lastXMovement === 0 && this.lastYMovement === 0) {
      this.announceInterval.stop();
      this.announceInterval = null;
      return;
    }
    const signX = Math.sign(this.lastXMovement);
    const signY = Math.sign(this.lastYMovement);
    speak(MOVEMENT_ANNOUNCE_LABELS.get(signX).get(signY));
    this.lastXMovement = this.lastYMovement = 0;
  }
}

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

interface Corner {
  el: HTMLDivElement;
  pt: Point;
  pointerId: number|null;
}

/**
 * View controller for review document crop area page.
 */
export class CropDocument extends Review {
  private imageFrame: HTMLDivElement;

  /**
   * Size of image frame.
   */
  private frameSize = new Size(0, 0);

  /**
   * The original size of the image to be cropped.
   */
  private imageOriginalSize: Size|null = null;

  /**
   * Space size coordinates in |this.corners_|. Will change when image client
   * area resized.
   */
  private cornerSpaceSize: Size|null = null;

  private cropAreaContainer: SVGElement;
  private cropArea: SVGPolygonElement;

  /**
   * Index of |ROTATION| as current photo rotation.
   */
  private rotation = 0;

  private initialCorners: Point[]|null = null;
  private corners: Corner[];

  constructor() {
    super(ViewName.CROP_DOCUMENT);

    this.imageFrame = dom.getFrom(this.root, '.review-frame', HTMLDivElement);

    this.cropAreaContainer =
        dom.getFrom(this.root, '.crop-area-container', SVGElement);

    this.cropArea = dom.getFrom(this.image, '.crop-area', SVGPolygonElement);

    /**
     * Coordinates of document with respect to |this.cornerSpaceSize_|.
     */
    this.corners = (() => {
      const ret = [];
      for (let i = 0; i < 4; i++) {
        const tpl = util.instantiateTemplate('#document-drag-point-template');
        ret.push({
          el: dom.getFrom(tpl, `.dot`, HTMLDivElement),
          pt: new Point(0, 0),
          pointerId: null,
        });
        this.image.appendChild(tpl);
      }
      return ret;
    })();

    const updateRotation = (newRotation) => {
      this.rotation = newRotation;
      this.updateImage();
      this.updateCornerElAriaLabel();
    };

    const clockwiseBtn = dom.getFrom(
        this.root, 'button[i18n-aria=rotate_clockwise_button]',
        HTMLButtonElement);
    clockwiseBtn.addEventListener('click', () => {
      updateRotation((this.rotation + 1) % ROTATIONS.length);
    });

    const counterclockwiseBtn = dom.getFrom(
        this.root, 'button[i18n-aria=rotate_counterclockwise_button]',
        HTMLButtonElement);
    counterclockwiseBtn.addEventListener('click', () => {
      updateRotation((this.rotation + ROTATIONS.length - 1) % ROTATIONS.length);
    });

    const cornerSize = (() => {
      const style = this.corners[0].el.computedStyleMap();
      const width = util.getStyleValueInPx(style, 'width');
      const height = util.getStyleValueInPx(style, 'height');
      return new Size(width, height);
    })();

    this.corners.forEach((corn) => {
      // Start dragging on one corner.
      corn.el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        assert(e.target === corn.el);
        this.setDragging(corn, assertInstanceof(e, PointerEvent).pointerId);
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
      const pressedKeyIndices = new Set<number>();
      let keyInterval = null;
      const clearKeydown = () => {
        if (keyInterval !== null) {
          keyInterval.stop();
          keyInterval = null;
        }
        pressedKeyIndices.clear();
      };
      const announcer = new MovementAnnouncer();

      corn.el.addEventListener('blur', () => {
        clearKeydown();
      });

      corn.el.addEventListener('keydown', (e) => {
        const keyIdx = getKeyIndex(e);
        if (keyIdx === -1 || pressedKeyIndices.has(keyIdx)) {
          return;
        }
        const move = () => {
          let announceMoveX = 0;
          let announceMoveY = 0;
          let moveX = 0;
          let moveY = 0;
          for (const keyIdx of pressedKeyIndices) {
            const announceMoveXY = KEY_MOVEMENTS[keyIdx];
            announceMoveX += announceMoveXY.x;
            announceMoveY += announceMoveXY.y;
            const moveXY = KEY_MOVEMENTS[(keyIdx + this.rotation) % 4];
            moveX += moveXY.x;
            moveY += moveXY.y;
          }
          announcer.updateMovement(announceMoveX, announceMoveY);
          const {x: curX, y: curY} = corn.pt;
          const nextPt = new Point(curX + moveX, curY + moveY);
          const validPt = this.mapToValidArea(corn, nextPt);
          if (validPt === null) {
            return;
          }
          corn.pt = validPt;
          this.updateCornerEl();
        };
        pressedKeyIndices.add(keyIdx);
        move();

        if (keyInterval === null) {
          const PRESS_TIMEOUT = 500;
          const HOLD_INTERVAL = 100;
          keyInterval = new util.DelayInterval(() => {
            move();
          }, PRESS_TIMEOUT, HOLD_INTERVAL);
        }
      });

      corn.el.addEventListener('keyup', (e) => {
        const keyIdx = getKeyIndex(e);
        if (keyIdx === -1) {
          return;
        }
        pressedKeyIndices.delete(keyIdx);
        if (pressedKeyIndices.size === 0) {
          clearKeydown();
        }
      });
    });

    // Stop dragging.
    for (const eventName of ['pointerup', 'pointerleave', 'pointercancel']) {
      this.image.addEventListener(eventName, (e) => {
        e.preventDefault();
        this.clearDragging(assertInstanceof(e, PointerEvent).pointerId);
      });
    }

    // Move drag corner.
    this.image.addEventListener('pointermove', (e) => {
      e.preventDefault();

      const pointerId = assertInstanceof(e, PointerEvent).pointerId;
      const corn = this.findDragging(pointerId);
      if (corn === null) {
        return;
      }
      assert(corn.el.classList.contains('dragging'));

      let dragX = e.offsetX;
      let dragY = e.offsetY;
      const target = assertInstanceof(e.target, HTMLElement);
      // The offsetX, offsetY of corners.el are measured from their own left,
      // top.
      if (this.corners.find(({el}) => el === target) !== undefined) {
        const style = target.attributeStyleMap;
        dragX += util.getStyleValueInPx(style, 'left') - cornerSize.width / 2;
        dragY += util.getStyleValueInPx(style, 'top') - cornerSize.height / 2;
      }

      const validPt = this.mapToValidArea(corn, new Point(dragX, dragY));
      if (validPt === null) {
        return;
      }
      corn.pt = validPt;
      this.updateCornerEl();
    });

    // Prevent contextmenu popup triggered by long touch.
    this.image.addEventListener('contextmenu', (e) => {
      if (e['pointerType'] === 'touch') {
        e.preventDefault();
      }
    });
  }

  /**
   * @param corners Initial guess from corner detector.
   * @return Returns new selected corners to be cropped and its rotation.
   */
  async reviewCropArea(corners: Point[]):
      Promise<{corners: Point[], rotation: Rotation}> {
    this.initialCorners = corners;
    this.cornerSpaceSize = null;
    await super.startReview({
      positive: new Options(
          new Option(I18nString.LABEL_CROP_DONE, {exitValue: true}),
          ),
      negative: new Options(),
    });
    const newCorners = this.corners.map(
        ({pt: {x, y}}) => new Point(
            x / this.cornerSpaceSize.width, y / this.cornerSpaceSize.height));
    return {corners: newCorners, rotation: ROTATIONS[this.rotation]};
  }

  private setDragging(corn: Corner, pointerId: number) {
    corn.el.classList.add('dragging');
    corn.pointerId = pointerId;
  }

  private findDragging(pointerId: number): Corner|null {
    return this.corners.find(({pointerId: id}) => id === pointerId) || null;
  }

  private clearDragging(pointerId: number) {
    const corn = this.findDragging(pointerId);
    if (corn === null) {
      return;
    }
    corn.el.classList.remove('dragging');
    corn.pointerId = null;
  }

  private mapToValidArea(corn: Corner, pt: Point): Point|null {
    pt = new Point(
        Math.max(Math.min(pt.x, this.cornerSpaceSize.width), 0),
        Math.max(Math.min(pt.y, this.cornerSpaceSize.height), 0));

    const idx = this.corners.findIndex((c) => c === corn);
    assert(idx !== -1);
    const prevPt = this.corners[(idx + 3) % 4].pt;
    const nextPt = this.corners[(idx + 1) % 4].pt;
    const restPt = this.corners[(idx + 2) % 4].pt;
    const closestDist =
        Math.min(this.cornerSpaceSize.width, this.cornerSpaceSize.height) *
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
    const box = new Box(assertInstanceof(this.cornerSpaceSize, Size));

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
     * @return Square distance of |pt3| to segment formed by |pt1| and |pt2|
     *     and the corresponding nearest point on the segment.
     */
    const distToSegment = (pt1: Point, pt2: Point, pt3: Point):
        {dist2: number, nearest: Point} => {
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

  private updateCornerEl() {
    const cords = this.corners.map(({pt: {x, y}}) => `${x},${y}`).join(' ');
    this.cropArea.setAttribute('points', cords);
    this.corners.forEach((corn) => {
      const style = corn.el.attributeStyleMap;
      style.set('left', CSS.px(corn.pt.x));
      style.set('top', CSS.px(corn.pt.y));
    });
  }

  private updateCornerElAriaLabel() {
    [I18nString.LABEL_DOCUMENT_TOP_LEFT_CORNER,
     I18nString.LABEL_DOCUMENT_BOTTOM_LEFT_CORNER,
     I18nString.LABEL_DOCUMENT_BOTTOM_RIGHT_CORNER,
     I18nString.LABEL_DOCUMENT_TOP_RIGHT_CORNER,
    ].forEach((label, index) => {
      const cornEl =
          this.corners[(this.rotation + index) % this.corners.length].el;
      cornEl.setAttribute('i18n-aria', label);
    });
    util.setupI18nElements(this.root);
  }

  /**
   * Updates image position/size with respect to |this.rotation_|,
   * |this.frameSize_| and |this.imageOriginalSize_|.
   */
  private updateImage() {
    const {width: frameW, height: frameH} = this.frameSize;
    const {width: rawImageW, height: rawImageH} = this.imageOriginalSize;
    const style = this.image.attributeStyleMap;

    let rotatedW = rawImageW;
    let rotatedH = rawImageH;
    if (ROTATIONS[this.rotation] === Rotation.ANGLE_90 ||
        ROTATIONS[this.rotation] === Rotation.ANGLE_270) {
      [rotatedW, rotatedH] = [rotatedH, rotatedW];
    }
    const scale = Math.min(1, frameW / rotatedW, frameH / rotatedH);
    const newImageW = scale * rawImageW;
    const newImageH = scale * rawImageH;
    style.set('width', CSS.px(newImageW));
    style.set('height', CSS.px(newImageH));
    this.cropAreaContainer.setAttribute(
        'viewBox', `0 0 ${newImageW} ${newImageH}`);

    // Update corner space.
    if (this.cornerSpaceSize === null) {
      this.initialCorners.forEach(({x, y}, idx) => {
        this.corners[idx].pt = new Point(x * newImageW, y * newImageH);
      });
      this.initialCorners = null;
    } else {
      const oldImageW = this.cornerSpaceSize?.width || newImageW;
      const oldImageH = this.cornerSpaceSize?.height || newImageH;
      this.corners.forEach((corn) => {
        corn.pt = new Point(
            corn.pt.x / oldImageW * newImageW,
            corn.pt.y / oldImageH * newImageH);
      });
    }
    this.cornerSpaceSize = new Size(newImageW, newImageH);

    const originX =
        frameW / 2 + rotatedW * scale / 2 * [-1, 1, 1, -1][this.rotation];
    const originY =
        frameH / 2 + rotatedH * scale / 2 * [-1, -1, 1, 1][this.rotation];
    style.set('left', CSS.px(originX));
    style.set('top', CSS.px(originY));

    const deg = ROTATIONS[this.rotation];
    style.set(
        'transform', new CSSTransformValue([new CSSRotate(CSS.deg(deg))]));

    this.updateCornerEl();
  }

  async setReviewPhoto(blob: Blob): Promise<void> {
    const image = new Image();
    await this.loadImage(image, blob);
    this.imageOriginalSize = new Size(image.width, image.height);
    const style = this.image.attributeStyleMap;
    if (style.has('background-image')) {
      const oldUrl = style.get('background-image')
                         .toString()
                         .match(/url\(['"]([^'"]+)['"]\)/)[1];
      URL.revokeObjectURL(oldUrl);
    }
    style.set('background-image', `url('${image.src}')`);

    this.rotation = 0;
    this.updateCornerElAriaLabel();
  }

  layout(): void {
    super.layout();

    const rect = this.imageFrame.getBoundingClientRect();
    this.frameSize = new Size(rect.width, rect.height);
    this.updateImage();
    // Clear all dragging corners.
    for (const corn of this.corners) {
      if (corn.pointerId !== null) {
        this.clearDragging(corn.pointerId);
      }
    }
  }
}
