// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Point} from '../constants.js';
import type {SignatureStrokePoint} from '../controller.js';
import {PluginController} from '../controller.js';

import {getCss} from './viewer_signature_dialog.css.js';
import {getHtml} from './viewer_signature_dialog.html.js';

const CANVAS_FILL_STYLE: string = 'rgb(227, 227, 227)';

// Constructs a Path2D from a list of closed polygon loops, where each loop is
// a flattened array of [x0, y0, x1, y1, ...] vertex coordinate pairs.
function buildPath2D(outlines: number[][]): Path2D {
  const path = new Path2D();
  for (const loop of outlines) {
    // Each loop must contain an even number of coordinates and at least two
    // vertices (x0, y0, x1, y1) to form a closed path.
    assert(loop.length >= 4 && loop.length % 2 === 0);
    // Move to the initial vertex (x0, y0).
    path.moveTo(loop[0]!, loop[1]!);
    for (let i = 2; i < loop.length - 1; i += 2) {
      path.lineTo(loop[i]!, loop[i + 1]!);
    }
    path.closePath();
  }
  return path;
}

export interface ViewerSignatureDialogElement {
  $: {
    canvas: HTMLCanvasElement,
    clear: CrButtonElement,
    create: CrButtonElement,
    dialog: CrDialogElement,
  };
}

export class ViewerSignatureDialogElement extends CrLitElement {
  static get is() {
    return 'viewer-signature-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hasStrokes_: {type: Boolean},
    };
  }

  protected accessor hasStrokes_: boolean = false;

  // Top-left client coordinates of `#canvas` while a stroke is in progress, or
  // `null` when not drawing.
  private canvasOffset_: Point|null = null;
  private finishedStrokes_: Path2D[] = [];
  private inProgressStroke_: Path2D|null = null;
  private pluginController_: PluginController = PluginController.getInstance();
  private strokeStartTime_: number = 0;

  protected onCrDialogOpen_() {
    const canvas = this.$.canvas;
    canvas.width = Math.round(canvas.clientWidth * window.devicePixelRatio);
    canvas.height = Math.round(canvas.clientHeight * window.devicePixelRatio);
  }

  // In the pointer event handlers below, `postMessage()` is called
  // synchronously before `await`, so the PDF content renderer processes and
  // replies to stroke messages in FIFO order.
  protected async onPointerdown_(e: PointerEvent) {
    if (e.button !== 0 || this.canvasOffset_) {
      return;
    }
    this.$.canvas.setPointerCapture(e.pointerId);
    // Cache the canvas offset once per stroke to pick up any window resize
    // between strokes without querying layout on every pointermove event.
    const rect = this.$.canvas.getBoundingClientRect();
    this.canvasOffset_ = {x: rect.left, y: rect.top};
    this.strokeStartTime_ = e.timeStamp;
    const reply = await this.pluginController_.startSignatureStroke(
        this.getPointFromEvent_(e));
    this.updateInProgressStroke_(reply.outlines);
  }

  protected async onPointermove_(e: PointerEvent) {
    if (!this.canvasOffset_) {
      return;
    }
    const coalesced = e.getCoalescedEvents();
    const events = coalesced.length > 0 ? coalesced : [e];
    const points = events.map(event => this.getPointFromEvent_(event));
    const reply = await this.pluginController_.continueSignatureStroke(points);
    this.updateInProgressStroke_(reply.outlines);
  }

  protected async onPointerup_(e: PointerEvent) {
    if (!this.canvasOffset_) {
      return;
    }
    const point = this.getPointFromEvent_(e);
    this.canvasOffset_ = null;
    this.strokeStartTime_ = 0;
    const reply = await this.pluginController_.finishSignatureStroke(point);
    this.finishStroke_(reply.outlines);
  }

  protected async onPointercancel_() {
    if (!this.canvasOffset_) {
      return;
    }
    this.canvasOffset_ = null;
    this.strokeStartTime_ = 0;
    const reply = await this.pluginController_.finishSignatureStroke();
    this.finishStroke_(reply.outlines);
  }

  protected onClearClick_() {
    this.hasStrokes_ = false;
    this.finishedStrokes_ = [];
    this.inProgressStroke_ = null;
    this.renderCanvas_();
  }

  protected onCreateClick_() {
    // TODO(crbug.com/573237639): Save the signature.
    this.$.dialog.close();
  }

  private getPointFromEvent_(e: PointerEvent): SignatureStrokePoint {
    assert(this.canvasOffset_);
    return {
      x: e.clientX - this.canvasOffset_.x,
      y: e.clientY - this.canvasOffset_.y,
      elapsedTime: e.timeStamp - this.strokeStartTime_,
    };
  }

  private updateInProgressStroke_(outlines: number[][]) {
    this.inProgressStroke_ = buildPath2D(outlines);
    this.renderCanvas_();
  }

  private finishStroke_(outlines: number[][]) {
    this.hasStrokes_ = true;
    this.finishedStrokes_.push(buildPath2D(outlines));
    this.inProgressStroke_ = null;
    this.renderCanvas_();
  }

  private renderCanvas_() {
    const canvas = this.$.canvas;
    const context = canvas.getContext('2d');
    assert(context);

    context.clearRect(0, 0, canvas.width, canvas.height);

    context.save();
    context.scale(window.devicePixelRatio, window.devicePixelRatio);
    context.fillStyle = CANVAS_FILL_STYLE;

    for (const stroke of this.finishedStrokes_) {
      context.fill(stroke, 'nonzero');
    }
    if (this.inProgressStroke_) {
      context.fill(this.inProgressStroke_, 'nonzero');
    }
    context.restore();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-signature-dialog': ViewerSignatureDialogElement;
  }
}

customElements.define(
    ViewerSignatureDialogElement.is, ViewerSignatureDialogElement);
