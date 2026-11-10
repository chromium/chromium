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

  // Top-left client coordinates of `#canvas` while a stroke is in progress, or
  // `null` when not drawing.
  private canvasOffset_: Point|null = null;
  private pluginController_: PluginController = PluginController.getInstance();
  private strokeStartTime_: number = 0;

  protected onPointerdown_(e: PointerEvent) {
    if (e.button !== 0 || this.canvasOffset_) {
      return;
    }
    this.$.canvas.setPointerCapture(e.pointerId);
    // Cache the canvas offset once per stroke to pick up any window resize
    // between strokes without querying layout on every pointermove event.
    const rect = this.$.canvas.getBoundingClientRect();
    this.canvasOffset_ = {x: rect.left, y: rect.top};
    this.strokeStartTime_ = e.timeStamp;
    this.pluginController_.startSignatureStroke(this.getPointFromEvent_(e));
  }

  protected onPointermove_(e: PointerEvent) {
    if (!this.canvasOffset_) {
      return;
    }
    const coalesced = e.getCoalescedEvents();
    const events = coalesced.length > 0 ? coalesced : [e];
    const points = events.map(event => this.getPointFromEvent_(event));
    this.pluginController_.continueSignatureStroke(points);
  }

  protected onPointerup_(e: PointerEvent) {
    if (!this.canvasOffset_) {
      return;
    }
    const point = this.getPointFromEvent_(e);
    this.canvasOffset_ = null;
    this.strokeStartTime_ = 0;
    this.pluginController_.finishSignatureStroke(point);
  }

  protected onPointercancel_() {
    if (!this.canvasOffset_) {
      return;
    }
    this.canvasOffset_ = null;
    this.strokeStartTime_ = 0;
    this.pluginController_.finishSignatureStroke();
  }

  private getPointFromEvent_(e: PointerEvent): SignatureStrokePoint {
    assert(this.canvasOffset_);
    return {
      x: e.clientX - this.canvasOffset_.x,
      y: e.clientY - this.canvasOffset_.y,
      elapsedTime: e.timeStamp - this.strokeStartTime_,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-signature-dialog': ViewerSignatureDialogElement;
  }
}

customElements.define(
    ViewerSignatureDialogElement.is, ViewerSignatureDialogElement);
