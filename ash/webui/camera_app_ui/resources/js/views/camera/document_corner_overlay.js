// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertString} from '../../assert.js';
import * as dom from '../../dom.js';
import {
  Point,
  PolarVector,
  vectorFromPoints,
} from '../../geometry.js';
import {I18nString} from '../../i18n_string.js';
import {DeviceOperator} from '../../mojo/device_operator.js';
import {
  closeEndpoint,
  MojoEndpoint,  // eslint-disable-line no-unused-vars
} from '../../mojo/util.js';
import * as toast from '../../toast.js';
import * as util from '../../util.js';

/**
 * Base length of line without scaling in px.
 */
const BASE_LENGTH = 100;

/**
 * @typedef {{
 *   position?: !Point,
 *   angle?: number,
 *   length?: number
 * }}
 */
let PlaceParams;  // eslint-disable-line no-unused-vars

/**
 * Controller for placing line-like element.
 */
class Line {
  /**
   * @param {!HTMLDivElement} el
   */
  constructor(el) {
    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.el_ = el;
  }

  /**
   * @param {PlaceParams} params
   *     'position' is the x, y coordinates of start endpoint in px.  'angle' is
   *     the rotate angle in rad.  'length' is the length of the line.
   */
  place({position, angle, length}) {
    const transforms = [];
    if (position !== undefined) {
      transforms.push(new CSSTranslate(CSS.px(position.x), CSS.px(position.y)));
    }
    if (angle !== undefined) {
      const prevAngle = this.angle_();
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
      this.el_.attributeStyleMap.set('width', CSS.px(BASE_LENGTH));
      const scale = length / BASE_LENGTH;
      transforms.push(new CSSScale(CSS.number(scale), CSS.number(1)));
    }
    this.el_.attributeStyleMap.set(
        'transform', new CSSTransformValue(transforms));
  }

  /**
   * @return {?CSSTransformValue}
   */
  getTransform_() {
    const trans = this.el_.attributeStyleMap.get('transform');
    return trans && assertInstanceof(trans, CSSTransformValue);
  }

  /**
   * @return {?number}
   */
  angle_() {
    const transforms = this.getTransform_();
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
  /**
   * @param {!HTMLDivElement} container
   */
  constructor(container) {
    const tpl = util.instantiateTemplate('#document-corner-template');

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.corner_ = dom.getFrom(tpl, `div.corner`, HTMLDivElement);

    /**
     * @const {!Line}
     * @private
     */
    this.prevLine_ =
        new Line(dom.getAllFrom(tpl, `div.line`, HTMLDivElement)[0]);

    /**
     * @const {!Line}
     * @private
     */
    this.nextLine_ =
        new Line(dom.getAllFrom(tpl, `div.line`, HTMLDivElement)[1]);

    container.appendChild(tpl);
  }

  /**
   * @param {!Point} pt
   * @param {!Point} prevPt
   * @param {!Point} nextPt
   */
  place(pt, prevPt, nextPt) {
    this.corner_.attributeStyleMap.set('left', CSS.px(pt.x));
    this.corner_.attributeStyleMap.set('top', CSS.px(pt.y));
    this.prevLine_.place(
        {angle: vectorFromPoints(prevPt, pt).cssRotateAngle()});
    this.nextLine_.place(
        {angle: vectorFromPoints(nextPt, pt).cssRotateAngle()});
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
  /**
   * @public
   */
  constructor() {
    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.overlay_ = dom.get('#preview-document-corner-overlay', HTMLDivElement);

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.noDocumentToast_ =
        dom.getFrom(this.overlay_, '.no-document-toast', HTMLDivElement);

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.cornerContainer_ =
        dom.getFrom(this.overlay_, '.corner-container', HTMLDivElement);

    /**
     * @type {?string}
     * @private
     */
    this.deviceId_ = null;

    /**
     * @type {?MojoEndpoint}
     * @private
     */
    this.observer_ = null;

    /**
     * @type {!Array<!Line>}
     * @private
     */
    this.sides_ = (() => {
      const lines = [];
      for (let i = 0; i < 4; i++) {
        const tpl = util.instantiateTemplate('#document-side-template');
        const el = dom.getFrom(tpl, `div`, HTMLDivElement);
        lines.push(new Line(el));
        this.cornerContainer_.appendChild(tpl);
      }
      return lines;
    })();

    /**
     * @type {!Array<!Corner>}
     * @private
     */
    this.corners_ = (() => {
      const corners = [];
      for (let i = 0; i < 4; i++) {
        corners.push(new Corner(this.cornerContainer_));
      }
      return corners;
    })();

    /**
     * @type {?number}
     * @private
     */
    this.noDocumentTimerId_ = null;

    this.hide_();
  }

  /**
   * Attaches to camera with specified device id.
   * @param {string} deviceId
   */
  attach(deviceId) {
    assert(this.deviceId_ === null);
    this.deviceId_ = deviceId;
  }

  /**
   * Detaches from previous attached camera.
   * @return {!Promise}
   */
  async detach() {
    await this.stop();
    this.deviceId_ = null;
  }

  /**
   * @return {!Promise}
   */
  async start() {
    if (this.observer_ !== null) {
      return;
    }
    const deviceOperator = await DeviceOperator.getInstance();
    if (deviceOperator === null) {
      // Skip showing indicator on fake camera.
      return;
    }
    this.observer_ = await deviceOperator.registerDocumentCornersObserver(
        assertString(this.deviceId_), (corners) => {
          if (corners.length === 0) {
            this.onNoCornerDetected_();
            return;
          }
          const rect = this.cornerContainer_.getBoundingClientRect();
          const toOverlaySpace = (pt) =>
              new Point(rect.width * pt.x, rect.height * pt.y);
          this.onCornerDetected_(corners.map(toOverlaySpace));
        });
    this.hide_();
    this.clearNoDocumentTimer_();
    this.setNoDocumentTimer_();
  }

  /**
   * @return {!Promise}
   */
  async stop() {
    if (this.observer_ === null) {
      return;
    }
    closeEndpoint(this.observer_);
    this.observer_ = null;
    this.hide_();
    this.clearNoDocumentTimer_();
  }

  /**
   * @return {boolean}
   */
  isEnabled() {
    return this.observer_ !== null;
  }

  /**
   * @private
   */
  onNoCornerDetected_() {
    this.hideIndicators_();
    if (this.isNoDocumentToastShown_()) {
      return;
    }
    if (this.noDocumentTimerId_ === null) {
      this.setNoDocumentTimer_();
    }
  }

  /**
   * @param {!Array<!Point>} corners
   */
  onCornerDetected_(corners) {
    this.hideNoDocumentToast_();
    this.clearNoDocumentTimer_();
    if (this.isIndicatorsShown_()) {
      this.updateCorners_(corners);
    } else {
      toast.speak(I18nString.MSG_DOCUMENT_DETECTED);
      this.showIndicators_();
      this.settleCorners_(corners);
    }
  }

  /**
   * Place first 4 corners on the overlay and play settle animation.
   * @param {!Array<!Point>} corners
   * @private
   */
  settleCorners_(corners) {
    /**
     * Start point(corner coordinates + outer shift) of settle animation.
     * @param {!Point} corn
     * @param {!Point} corn2
     * @param {!Point} corn3
     * @param {number} d
     * @return {!Point}
     */
    const calSettleStart = (corn, corn2, corn3, d) => {
      const side = vectorFromPoints(corn2, corn);
      const norm = side.normal().multiply(d);

      const side2 = vectorFromPoints(corn2, corn3);
      const angle = side.rotation(side2);
      const dir = side.direction().multiply(d / Math.tan(angle / 2));

      return vectorFromPoints(corn2).add(norm).add(dir).point();
    };
    const starts = corners.map((_, idx) => {
      const prevIdx = (idx + 3) % 4;
      const nextIdx = (idx + 1) % 4;
      return calSettleStart(
          corners[prevIdx], corners[idx], corners[nextIdx], 50);
    });

    // Set start of dot transition.
    starts.forEach((corn, idx) => {
      const prevIdx = (idx + 3) % 4;
      const nextIdx = (idx + 1) % 4;
      this.corners_[idx].place(corn, starts[prevIdx], starts[nextIdx]);
    });

    // Set start of line transition.
    this.sides_.forEach((line, i) => {
      const startCorn = starts[i];
      const startCorn2 = starts[(i + 1) % 4];
      const startSide = vectorFromPoints(startCorn2, startCorn);
      line.place({
        position: startCorn,
        angle: startSide.cssRotateAngle(),
        length: startSide.length(),
      });
    });

    /** @suppress {suspiciousCode} */
    this.cornerContainer_.offsetParent;  // Force start state of transition.

    // Set end of dot transition.
    corners.forEach((corn, i) => {
      const prevIdx = (i + 3) % 4;
      const nextIdx = (i + 1) % 4;
      this.corners_[i].place(corn, corners[prevIdx], corners[nextIdx]);
    });

    this.sides_.forEach((line, i) => {
      const endCorn = corners[i];
      const endCorn2 = corners[(i + 1) % 4];
      const endSide = vectorFromPoints(endCorn2, endCorn);
      line.place({
        position: endCorn,
        angle: endSide.cssRotateAngle(),
        length: endSide.length(),
      });
    });
  }

  /**
   * Place first 4 corners on the overlay and play settle animation.
   * @param {!Array<!Point>} corners
   * @private
   */
  updateCorners_(corners) {
    corners.forEach((corn, i) => {
      const prevIdx = (i + 3) % 4;
      const nextIdx = (i + 1) % 4;
      this.corners_[i].place(corn, corners[prevIdx], corners[nextIdx]);
    });
    this.sides_.forEach((line, i) => {
      const corn = corners[i];
      const corn2 = corners[(i + 1) % 4];
      const side = vectorFromPoints(corn2, corn);
      line.place({
        position: corn,
        angle: side.cssRotateAngle(),
        length: side.length(),
      });
    });
  }

  /**
   * Hides overlay related UIs.
   * @private
   */
  hide_() {
    this.hideIndicators_();
    this.hideNoDocumentToast_();
  }

  /**
   * @private
   * @return {boolean}
   */
  isIndicatorsShown_() {
    return this.overlay_.classList.contains('show-corner-indicator');
  }

  /**
   * @private
   */
  showIndicators_() {
    this.overlay_.classList.add('show-corner-indicator');
  }

  /**
   * @private
   */
  hideIndicators_() {
    this.overlay_.classList.remove('show-corner-indicator');
  }

  /**
   * @private
   */
  showNoDocumentToast_() {
    this.noDocumentToast_.attributeStyleMap.delete('visibility');
  }

  /**
   * @private
   */
  hideNoDocumentToast_() {
    this.noDocumentToast_.attributeStyleMap.set('visibility', 'hidden');
  }

  /**
   * @private
   * @return {boolean}
   */
  isNoDocumentToastShown_() {
    return !this.noDocumentToast_.attributeStyleMap.has('visibility');
  }

  /**
   * @private
   */
  setNoDocumentTimer_() {
    if (this.noDocumentTimerId_ !== null) {
      clearTimeout(this.noDocumentTimerId_);
    }
    this.noDocumentTimerId_ = setTimeout(() => {
      this.showNoDocumentToast_();
      this.clearNoDocumentTimer_();
    }, SHOW_NO_DOCUMENT_TOAST_TIMEOUT_MS);
  }

  /**
   * @private
   */
  clearNoDocumentTimer_() {
    if (this.noDocumentTimerId_ !== null) {
      clearTimeout(this.noDocumentTimerId_);
      this.noDocumentTimerId_ = null;
    }
  }
}
