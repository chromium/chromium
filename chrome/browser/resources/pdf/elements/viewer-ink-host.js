// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PDFMetrics} from '../metrics.js';
import {Viewport} from '../viewport.js';

/** @enum {string} */
const State = {
  LOADING: 'loading',
  ACTIVE: 'active',
  IDLE: 'idle'
};

const BACKGROUND_COLOR = '#525659';

/**
 * Hosts the Ink component which is responsible for both PDF rendering and
 * annotation when in annotation mode.
 */
Polymer({
  is: 'viewer-ink-host',

  _template: html`{__html_template__}`,

  /** @private {InkAPI} */
  ink_: null,

  /** @private {?string} */
  fileName_: null,

  /** @private {ArrayBuffer} */
  buffer_: null,

  /** @private {State} */
  state_: State.IDLE,

  /** @private {PointerEvent} */
  activePointer_: null,

  /** @private {?number} */
  lastZoom_: null,

  /**
   * Used to conditionally allow a 'touchstart' event to cause
   * a gesture. If we receive a 'touchstart' with this timestamp
   * we will skip calling `preventDefault()`.
   * @private {?number}
   */
  allowTouchStartTimeStamp_: null,

  /** @private {boolean} */
  penMode_: false,

  /** @type {?Viewport} */
  viewport: null,

  /** @type {?AnnotationTool} */
  tool_: null,

  /**
   * Whether we should suppress pointer events due to a gesture,
   * eg. pinch-zoom.
   *
   * @private {boolean}
   */
  pointerGesture_: false,

  listeners: {
    pointerdown: 'onPointerDown_',
    pointerup: 'onPointerUpOrCancel_',
    pointermove: 'onPointerMove_',
    pointercancel: 'onPointerUpOrCancel_',
    pointerleave: 'onPointerLeave_',
    touchstart: 'onTouchStart_',
  },

  /** Turns off pen mode if it is active. */
  resetPenMode() {
    this.penMode_ = false;
  },

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    this.tool_ = tool;
    if (this.state_ == State.ACTIVE) {
      this.ink_.setAnnotationTool(tool);
    }
  },

  /** @param {PointerEvent} e */
  isActivePointer_: function(e) {
    return this.activePointer_ && this.activePointer_.pointerId == e.pointerId;
  },

  /**
   * Dispatches a pointer event to Ink.
   *
   * @param {PointerEvent} e
   */
  dispatchPointerEvent_: function(e) {
    // TODO(dstockwell) come up with a solution to propagate e.timeStamp.
    this.ink_.dispatchPointerEvent(e.type, {
      pointerId: e.pointerId,
      pointerType: e.pointerType,
      clientX: e.clientX,
      clientY: e.clientY,
      pressure: e.pressure,
      buttons: e.buttons,
    });
  },

  /** @param {TouchEvent} e */
  onTouchStart_: function(e) {
    if (e.timeStamp !== this.allowTouchStartTimeStamp_) {
      e.preventDefault();
    }
    this.allowTouchStartTimeStamp_ = null;
  },

  /** @param {PointerEvent} e */
  onPointerDown_: function(e) {
    if (e.pointerType == 'mouse' && e.buttons != 1 || this.pointerGesture_) {
      return;
    }

    if (e.pointerType == 'pen') {
      this.penMode_ = true;
    }

    if (this.activePointer_) {
      if (this.activePointer_.pointerType == 'touch' &&
          e.pointerType == 'touch') {
        // A multi-touch gesture has started with the active pointer. Cancel
        // the active pointer and suppress further events until it is released.
        this.pointerGesture_ = true;
        this.ink_.dispatchPointerEvent('pointercancel', {
          pointerId: this.activePointer_.pointerId,
          pointerType: this.activePointer_.pointerType,
        });
      }
      return;
    }

    if (!this.viewport.isPointInsidePage({x: e.clientX, y: e.clientY}) &&
        (e.pointerType == 'touch' || e.pointerType == 'pen')) {
      // If a touch or pen is outside the page, we allow pan gestures to start.
      this.allowTouchStartTimeStamp_ = e.timeStamp;
      return;
    }

    if (e.pointerType == 'touch' && this.penMode_) {
      // If we see a touch after having seen a pen, we allow touches to start
      // pan gestures anywhere and suppress all touches from drawing.
      this.allowTouchStartTimeStamp_ = e.timeStamp;
      return;
    }

    this.activePointer_ = e;
    this.dispatchPointerEvent_(e);
  },

  /** @param {PointerEvent} e */
  onPointerLeave_: function(e) {
    if (e.pointerType != 'mouse' || !this.isActivePointer_(e)) {
      return;
    }
    this.onPointerUpOrCancel_(new PointerEvent('pointerup', e));
  },

  /** @param {PointerEvent} e */
  onPointerUpOrCancel_: function(e) {
    if (!this.isActivePointer_(e)) {
      return;
    }
    this.activePointer_ = null;
    if (!this.pointerGesture_) {
      this.dispatchPointerEvent_(e);
      // If the stroke was not cancelled (type == pointercanel),
      // notify about mutation and record metrics.
      if (e.type == 'pointerup') {
        this.dispatchEvent(new CustomEvent('stroke-added'));
        if (e.pointerType == 'mouse') {
          PDFMetrics.record(PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_MOUSE);
        } else if (e.pointerType == 'pen') {
          PDFMetrics.record(PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_PEN);
        } else if (e.pointerType == 'touch') {
          PDFMetrics.record(PDFMetrics.UserAction.ANNOTATE_STROKE_DEVICE_TOUCH);
        }
        if (this.tool_.tool == 'eraser') {
          PDFMetrics.record(PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_ERASER);
        } else if (this.tool_.tool == 'pen') {
          PDFMetrics.record(PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_PEN);
        } else if (this.tool_.tool == 'highlighter') {
          PDFMetrics.record(
              PDFMetrics.UserAction.ANNOTATE_STROKE_TOOL_HIGHLIGHTER);
        }
      }
    }
    this.pointerGesture_ = false;
  },

  /** @param {PointerEvent} e */
  onPointerMove_: function(e) {
    if (!this.isActivePointer_(e) || this.pointerGesture_) {
      return;
    }

    let events = e.getCoalescedEvents();
    if (events.length == 0) {
      events = [e];
    }
    for (const event of events) {
      this.dispatchPointerEvent_(event);
    }
  },

  /**
   * Begins annotation mode with the document represented by `data`.
   * When the return value resolves the Ink component will be ready
   * to render immediately.
   *
   * @param {string} fileName The name of the PDF file.
   * @param {!ArrayBuffer} data The contents of the PDF document.
   * @return {!Promise} void value.
   */
  load: async function(fileName, data) {
    this.fileName_ = fileName;
    this.state_ = State.LOADING;
    this.$.frame.src = 'ink/index.html';
    await new Promise(resolve => this.$.frame.onload = resolve);
    this.ink_ = await this.$.frame.contentWindow.initInk();
    this.ink_.addUndoStateListener(
        e => this.dispatchEvent(
            new CustomEvent('undo-state-changed', {detail: e})));
    this.ink_.setPDF(data);
    this.state_ = State.ACTIVE;
    this.viewportChanged();
    // Wait for the next task to avoid a race where Ink drops the background
    // color.
    await new Promise(resolve => setTimeout(resolve));
    this.ink_.setOutOfBoundsColor(BACKGROUND_COLOR);
    const spacing = Viewport.PAGE_SHADOW.top + Viewport.PAGE_SHADOW.bottom;
    this.ink_.setPageSpacing(spacing);
    this.style.visibility = 'visible';
  },

  viewportChanged: function() {
    if (this.state_ != State.ACTIVE) {
      return;
    }
    const viewport = this.viewport;
    const pos = viewport.position;
    const size = viewport.size;
    const zoom = viewport.getZoom();
    const documentWidth = viewport.getDocumentDimensions().width * zoom;
    // Adjust for page shadows.
    const y = pos.y - Viewport.PAGE_SHADOW.top * zoom;
    let x = pos.x - Viewport.PAGE_SHADOW.left * zoom;
    // Center the document if the width is smaller than the viewport.
    if (documentWidth < size.width) {
      x += (documentWidth - size.width) / 2;
    }
    // Invert the Y-axis and convert Pixels to Points.
    const pixelsToPoints = 72 / 96;
    const scale = pixelsToPoints / zoom;
    const camera = {
      top: (-y) * scale,
      left: (x) * scale,
      right: (x + size.width) * scale,
      bottom: (-y - size.height) * scale,
    };
    // Ink doesn't scale the shadow, so we must update it each time the zoom
    // changes.
    if (this.lastZoom_ !== zoom) {
      this.lastZoom_ = zoom;
      this.updateShadow_(zoom);
    }
    this.ink_.setCamera(camera);
  },

  /** Undo the last edit action. */
  undo() {
    this.ink_.undo();
  },

  /** Redo the last undone edit action. */
  redo() {
    this.ink_.redo();
  },

  /**
   * @return {!Promise<{fileName: string, dataToSave: ArrayBuffer}>}
   *     The serialized PDF document including any annotations that were made.
   */
  saveDocument: async function() {
    if (this.state_ == State.ACTIVE) {
      this.buffer_ = await this.ink_.getPDFDestructive().buffer;
      this.state_ = State.IDLE;
    }
    return {
      fileName: /** @type {string} */ (this.fileName_),
      dataToSave: this.buffer_,
    };
  },

  /** @param {number} zoom */
  updateShadow_(zoom) {
    const boxWidth = (50 * zoom) |0;
    const shadowWidth = (8 * zoom) |0;
    const width = boxWidth + shadowWidth * 2 + 2;
    const boxOffset = (width - boxWidth) / 2;

    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = width;

    const ctx = canvas.getContext('2d');
    ctx.fillStyle = 'black';
    ctx.shadowColor = 'black';
    ctx.shadowBlur = shadowWidth;
    ctx.fillRect(boxOffset, boxOffset, boxWidth, boxWidth);
    ctx.shadowBlur = 0;

    // 9-piece markers
    for (let i = 0; i < 4; i++) {
      ctx.fillStyle = 'white';
      ctx.fillRect(0, 0, width, 1);
      ctx.fillStyle = 'black';
      ctx.fillRect(shadowWidth + 1, 0, boxWidth, 1);
      ctx.rotate(0.5 * Math.PI);
      ctx.translate(0, -width);
    }

    this.ink_.setBorderImage(canvas.toDataURL());
  }
});
