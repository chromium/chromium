// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertString} from '../../assert.js';
import * as dom from '../../dom.js';
import {
  Point,
  PolarVector,
  Vector,
  vectorFromPoints,
} from '../../geometry.js';
import {I18nString} from '../../i18n_string.js';
import {DeviceOperator} from '../../mojo/device_operator.js';
import {
  PointF,
} from '../../mojo/type.js';
import {
  closeEndpoint,
  MojoEndpoint,
} from '../../mojo/util.js';
import {speak} from '../../spoken_msg.js';
import * as util from '../../util.js';
import * as state from '../../state.js';

/**
 * Base length of line without scaling in px.
 */
const BASE_LENGTH = 100;

/**
 * Threshold of the document area scale difference.
 */
const THRESHOLD_SCALE_DIFF = 0.3;

/**
 * Information to roughly represents the area of the document displaying on the
 * stream.
 * The |center| is the center point of the detected document area and the
 * |scale| is calculated by the longest length among the document edges.
 */
interface DocumentArea {
  center: Point;
  scale: number;
}

/**
 * Controller for placing line-like element.
 */
class Line {
  constructor(private readonly el: HTMLDivElement) {}

  /**
   * @param params Place parameters.
   * @param params.position The x, y coordinates of start endpoint in px.
   * @param params.angle The rotate angle in rad.
   * @param params.length The length of the line.
   */
  place({position, angle, length}: {
    position?: Point,
    angle?: number,
    length?: number,
  }) {
    const transforms = [];
    if (position !== undefined) {
      transforms.push(new CSSTranslate(CSS.px(position.x), CSS.px(position.y)));
    }
    if (angle !== undefined) {
      const prevAngle = this.angle();
      if (prevAngle !== null) {
        // Derive new angle from prevAngle + smallest rotation angle between new
        // and prev to ensure the rotation transition like -pi to pi won't jump
        // too much.
        angle = prevAngle -
            new PolarVector(angle, 1).rotation(new PolarVector(prevAngle, 1));
      }
      transforms.push(new CSSRotate(CSS.rad(angle)));
    }
    if (length !== undefined) {
      // To prevent floating point precision error during transform scale
      // calculation. Scale from a larger base length instead of from 1px. See
      // b/194264574.
      this.el.attributeStyleMap.set('width', CSS.px(BASE_LENGTH));
      const scale = length / BASE_LENGTH;
      transforms.push(new CSSScale(CSS.number(scale), CSS.number(1)));
    }
    this.el.attributeStyleMap.set(
        'transform', new CSSTransformValue(transforms));
  }

  private getTransform(): CSSTransformValue|null {
    if (this.el.attributeStyleMap.has('transform')) {
      // Note that Chrome returns null instead of undefined when the value is
      // not found, which is different to the spec & TypeScript type. See
      // crbug.com/1291286.
      const trans = this.el.attributeStyleMap.get('transform');
      return assertInstanceof(trans, CSSTransformValue);
    }
    return null;
  }

  private angle(): number|null {
    const transforms = this.getTransform();
    if (transforms === null) {
      return null;
    }
    for (const transform of transforms) {
      if (transform instanceof CSSRotate) {
        return transform.angle.to('rad').value;
      }
    }
    return null;
  }
}

/**
 * Controller for placing corner indicator on preview overlay.
 */
class Corner {
  private readonly corner: HTMLDivElement;

  private readonly prevLine: Line;

  private readonly nextLine: Line;

  constructor(container: HTMLDivElement) {
    const tpl = util.instantiateTemplate('#document-corner-template');

    this.corner = dom.getFrom(tpl, `div.corner`, HTMLDivElement);
    this.prevLine =
        new Line(dom.getAllFrom(tpl, `div.line`, HTMLDivElement)[0]);
    this.nextLine =
        new Line(dom.getAllFrom(tpl, `div.line`, HTMLDivElement)[1]);

    container.appendChild(tpl);
  }

  place(pt: Point, prevPt: Point, nextPt: Point): void {
    this.corner.attributeStyleMap.set('left', CSS.px(pt.x));
    this.corner.attributeStyleMap.set('top', CSS.px(pt.y));
    this.prevLine.place({angle: vectorFromPoints(prevPt, pt).cssRotateAngle()});
    this.nextLine.place({angle: vectorFromPoints(nextPt, pt).cssRotateAngle()});
  }
}

/**
 * Timeout to show toast message when no document is detected within the time.
 */
const SHOW_NO_DOCUMENT_TOAST_TIMEOUT_MS = 4000;

/**
 * An overlay to show document corner rectangles over preview.
 */
export class DocumentCornerOverlay {
  private readonly overlay =
      dom.get('#preview-document-corner-overlay', HTMLDivElement);

  private readonly noDocumentToast: HTMLDivElement;

  private readonly cornerContainer: HTMLDivElement;

  private deviceId: string|null = null;

  private observer: MojoEndpoint|null = null;

  private readonly sides: Line[];

  private readonly corners: Corner[];

  private noDocumentTimerId: number|null = null;

  /**
   * Previous document area which are used to calculate the point of interest.
   */
  private prevDocArea: DocumentArea|null = null;

  /**
   * @param updatePointOfInterest Function to update point of interest on the
   *     stream.
   */
  constructor(
      private readonly updatePointOfInterest: (point: Point) => Promise<void>) {
    this.noDocumentToast =
        dom.getFrom(this.overlay, '.no-document-toast', HTMLDivElement);
    this.cornerContainer =
        dom.getFrom(this.overlay, '.corner-container', HTMLDivElement);
    this.sides = (() => {
      const lines = [];
      for (let i = 0; i < 4; i++) {
        const tpl = util.instantiateTemplate('#document-side-template');
        const el = dom.getFrom(tpl, `div`, HTMLDivElement);
        lines.push(new Line(el));
        this.cornerContainer.appendChild(tpl);
      }
      return lines;
    })();

    this.corners = (() => {
      const corners = [];
      for (let i = 0; i < 4; i++) {
        corners.push(new Corner(this.cornerContainer));
      }
      return corners;
    })();

    this.hide();
  }

  /**
   * Attaches to camera with specified device id.
   */
  attach(deviceId: string): void {
    assert(this.deviceId === null);
    this.deviceId = deviceId;
  }

  /**
   * Detaches from previous attached camera.
   */
  detach(): void {
    this.stop();
    this.deviceId = null;
  }

  async start(): Promise<void> {
    if (this.observer !== null) {
      return;
    }
    const deviceOperator = DeviceOperator.getInstance();
    if (deviceOperator === null) {
      // Skip showing indicator on fake camera.
      return;
    }
    this.observer = await deviceOperator.registerDocumentCornersObserver(
        assertString(this.deviceId), (corners) => {
          if (corners.length === 0) {
            this.onNoCornerDetected();
            return;
          }
          // Updating POI shouldn't block showing the new document corner
          // indicators, and multiple updates to POI can be called at the same
          // time (the new one will override the old one).
          void this.maybeUpdatePointOfInterest(corners);
          const rect = this.cornerContainer.getBoundingClientRect();
          function toOverlaySpace(pt: Point) {
            return new Point(rect.width * pt.x, rect.height * pt.y);
          }
          this.onCornerDetected(corners.map(toOverlaySpace));
        });
    this.hide();
    this.clearNoDocumentTimer();
    this.setNoDocumentTimer();
  }

  stop(): void {
    if (this.observer === null) {
      return;
    }
    closeEndpoint(this.observer);
    this.observer = null;
    this.hide();
    this.clearNoDocumentTimer();
    state.set(state.State.ENABLE_SCAN_DOCUMENT, false);
  }

  isEnabled(): boolean {
    return this.observer !== null;
  }

  private onNoCornerDetected() {
    this.hideIndicators();
    if (this.isNoDocumentToastShown()) {
      return;
    }
    if (this.noDocumentTimerId === null) {
      this.setNoDocumentTimer();
    }
  }

  private async maybeUpdatePointOfInterest(corners: PointF[]): Promise<void> {
    assert(corners.length === 4);

    const newDocArea = (() => {
      let centerX = 0;
      let centerY = 0;
      let maxEdgeLength = 0;
      const shouldUpdatePoi = (() => {
        let isPreviousPoiOutsideNewDoc = this.prevDocArea === null;
        const {x: xp, y: yp} = this.prevDocArea?.center ?? {x: 0, y: 0};
        for (let i = 0; i < corners.length; ++i) {
          const {x: x1, y: y1} = corners[i];
          const {x: x2, y: y2} = corners[(i + 1) % 4];

          centerX += x1 / 4;
          centerY += y1 / 4;

          const edgeLength = (new Vector(x2 - x1, y2 - y1)).length();
          maxEdgeLength = Math.max(maxEdgeLength, edgeLength);

          const d = (x2 - x1) * (yp - y1) - (xp - x1) * (y2 - y1);
          if (d >= 0) {
            isPreviousPoiOutsideNewDoc = true;
          }
        }
        const isDocScaleChanges = this.prevDocArea === null ||
            Math.abs(maxEdgeLength - this.prevDocArea.scale) /
                    this.prevDocArea.scale >
                THRESHOLD_SCALE_DIFF;
        return isPreviousPoiOutsideNewDoc || isDocScaleChanges;
      })();
      if (!shouldUpdatePoi) {
        return null;
      }
      return {center: new Point(centerX, centerY), scale: maxEdgeLength};
    })();

    if (newDocArea !== null) {
      try {
        await this.updatePointOfInterest(newDocArea.center);
      } catch {
        // POI might not be supported on device so it is acceptable to fail.
      }
      this.prevDocArea = newDocArea;
    }
  }

  private onCornerDetected(corners: Point[]) {
    this.hideNoDocumentToast();
    this.clearNoDocumentTimer();
    if (this.isIndicatorsShown()) {
      this.updateCorners(corners);
    } else {
      speak(I18nString.MSG_DOCUMENT_DETECTED);
      this.showIndicators();
      this.settleCorners(corners);
    }
  }

  /**
   * Place first 4 corners on the overlay and play settle animation.
   */
  private settleCorners(corners: Point[]) {
    /**
     * Start point(corner coordinates + outer shift) of settle animation.
     */
    function calculateSettleStart(
        corner: Point, corner2: Point, corner3: Point, d: number): Point {
      const side = vectorFromPoints(corner2, corner);
      const norm = side.normal().multiply(d);

      const side2 = vectorFromPoints(corner2, corner3);
      const angle = side.rotation(side2);
      const dir = side.direction().multiply(d / Math.tan(angle / 2));

      return vectorFromPoints(corner2).add(norm).add(dir).point();
    }
    const starts = corners.map((_, idx) => {
      const prevIdx = (idx + 3) % 4;
      const nextIdx = (idx + 1) % 4;
      return calculateSettleStart(
          corners[prevIdx], corners[idx], corners[nextIdx], 50);
    });

    // Set start of dot transition.
    for (const [idx, corner] of starts.entries()) {
      const prevIdx = (idx + 3) % 4;
      const nextIdx = (idx + 1) % 4;
      this.corners[idx].place(corner, starts[prevIdx], starts[nextIdx]);
    }

    // Set start of line transition.
    for (const [i, line] of this.sides.entries()) {
      const startCorner = starts[i];
      const startCorner2 = starts[(i + 1) % 4];
      const startSide = vectorFromPoints(startCorner2, startCorner);
      line.place({
        position: startCorner,
        angle: startSide.cssRotateAngle(),
        length: startSide.length(),
      });
    }

    void this.cornerContainer.offsetParent;  // Force start state of transition.

    // Set end of dot transition.
    for (const [i, corner] of corners.entries()) {
      const prevIdx = (i + 3) % 4;
      const nextIdx = (i + 1) % 4;
      this.corners[i].place(corner, corners[prevIdx], corners[nextIdx]);
    }

    for (const [i, line] of this.sides.entries()) {
      const endCorner = corners[i];
      const endCorner2 = corners[(i + 1) % 4];
      const endSide = vectorFromPoints(endCorner2, endCorner);
      line.place({
        position: endCorner,
        angle: endSide.cssRotateAngle(),
        length: endSide.length(),
      });
    }
  }

  /**
   * Place first 4 corners on the overlay and play settle animation.
   */
  private updateCorners(corners: Point[]) {
    for (const [i, corner] of corners.entries()) {
      const prevIdx = (i + 3) % 4;
      const nextIdx = (i + 1) % 4;
      this.corners[i].place(corner, corners[prevIdx], corners[nextIdx]);
    }
    for (const [i, line] of this.sides.entries()) {
      const corner = corners[i];
      const corner2 = corners[(i + 1) % 4];
      const side = vectorFromPoints(corner2, corner);
      line.place({
        position: corner,
        angle: side.cssRotateAngle(),
        length: side.length(),
      });
    }
  }

  /**
   * Hides overlay related UIs.
   */
  private hide() {
    this.hideIndicators();
    this.hideNoDocumentToast();
  }

  private isIndicatorsShown(): boolean {
    return this.overlay.classList.contains('show-corner-indicator');
  }

  private showIndicators() {
    this.overlay.classList.add('show-corner-indicator');
  }

  private hideIndicators() {
    this.overlay.classList.remove('show-corner-indicator');
  }

  private showNoDocumentToast() {
    this.noDocumentToast.attributeStyleMap.delete('visibility');
  }

  private hideNoDocumentToast() {
    this.noDocumentToast.attributeStyleMap.set('visibility', 'hidden');
  }

  private isNoDocumentToastShown(): boolean {
    return !this.noDocumentToast.attributeStyleMap.has('visibility');
  }

  private setNoDocumentTimer() {
    if (this.noDocumentTimerId !== null) {
      clearTimeout(this.noDocumentTimerId);
    }
    this.noDocumentTimerId = setTimeout(() => {
      this.showNoDocumentToast();
      this.clearNoDocumentTimer();
    }, SHOW_NO_DOCUMENT_TOAST_TIMEOUT_MS);
  }

  private clearNoDocumentTimer() {
    if (this.noDocumentTimerId !== null) {
      clearTimeout(this.noDocumentTimerId);
      this.noDocumentTimerId = null;
    }
  }
}
