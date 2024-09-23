// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {AnnotationTool} from '../annotation_tool.js';
import type {InkApi} from '../ink/ink_api.js';
import {record, UserAction} from '../metrics.js';
import type {Viewport} from '../viewport.js';
import {PAGE_SHADOW} from '../viewport.js';

import {getTemplate} from './viewer_ink_host.html.js';

enum State {
  LOADING = 'loading',
  ACTIVE = 'active',
  IDLE = 'idle'
}

const BACKGROUND_COLOR: string = '#525659';

declare global {
  interface HTMLElementEventMap {
    'undo-state-changed': CustomEvent<drawings.UndoState>;
  }
}

/**
 * Hosts the Ink component which is responsible for both PDF rendering and
 * annotation when in annotation mode.
 */
export class ViewerInkHostElement extends PolymerElement {
  static get is() {
    return 'viewer-ink-host';
  }

  static get template() {
    return getTemplate();
  }

  private tool_: AnnotationTool|null = null;
  viewport: Viewport|null = null;
  private activePointer_: PointerEvent|null = null;

  /**
   * Used to conditionally allow a 'touchstart' event to cause
   * a gesture. If we receive a 'touchstart' with this timestamp
   * we will skip calling `preventDefault()`.
   */
  private allowTouchStartTimeStamp_: number|null = null;
  private buffer_: ArrayBuffer|null = null;
  private fileName_: string|null = null;
  private ink_: InkApi|null = null;
  private lastZoom_: number|null = null;
  private penMode_: boolean = false;

  /**
   * Whether we should suppress pointer events due to a gesture;
   * eg. pinch-zoom.
   */
  private pointerGesture_: boolean = false;
  private state_: State = State.IDLE;

  override ready() {
    super.ready();
    this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
    this.addEventListener('pointerup', this.onPointerUpOrCancel_.bind(this));
    this.addEventListener('pointermove', this.onPointerMove_.bind(this));
    this.addEventListener(
        'pointercancel', this.onPointerUpOrCancel_.bind(this));
    this.addEventListener('pointerleave', this.onPointerLeave_.bind(this));
    this.addEventListener('touchstart', this.onTouchStart.bind(this));
  }

  /** Turns off pen mode if it is active. */
  resetPenMode() {
    this.penMode_ = false;
  }

  setAnnotationTool(tool: AnnotationTool) {
    this.tool_ = tool;
    if (this.state_ === State.ACTIVE) {
      this.ink_!.setAnnotationTool(tool);
    }
  }

  private isActivePointer_(e: PointerEvent) {
    return this.activePointer_ && this.activePointer_.pointerId === e.pointerId;
  }

  /** Dispatches a pointer event to Ink */
  private dispatchPointerEvent_(e: PointerEvent) {
    this.ink_!.dispatchPointerEvent(e);
  }

  onTouchStart(e: TouchEvent) {
    if (e.timeStamp !== this.allowTouchStartTimeStamp_) {
      e.preventDefault();
    }
    this.allowTouchStartTimeStamp_ = null;
  }

  private onPointerDown_(e: PointerEvent) {
    if (e.pointerType === 'mouse' && e.buttons !== 1 || this.pointerGesture_) {
      return;
    }

    if (e.pointerType === 'pen') {
      this.penMode_ = true;
    }

    if (this.activePointer_) {
      if (this.activePointer_.pointerType === 'touch' &&
          e.pointerType === 'touch') {
        // A multi-touch gesture has started with the active pointer. Cancel
        // the active pointer and suppress further events until it is released.
        this.pointerGesture_ = true;
        this.ink_!.dispatchPointerEvent(new PointerEvent('pointercancel', {
          pointerId: this.activePointer_.pointerId,
          pointerType: this.activePointer_.pointerType,
        }));
      }
      return;
    }

    if (!this.viewport!.isPointInsidePage({x: e.clientX, y: e.clientY}) &&
        (e.pointerType === 'touch' || e.pointerType === 'pen')) {
      // If a touch or pen is outside the page, we allow pan gestures to start.
      this.allowTouchStartTimeStamp_ = e.timeStamp;
      return;
    }

    if (e.pointerType === 'touch' && this.penMode_) {
      // If we see a touch after having seen a pen, we allow touches to start
      // pan gestures anywhere and suppress all touches from drawing.
      this.allowTouchStartTimeStamp_ = e.timeStamp;
      return;
    }

    this.activePointer_ = e;
    this.dispatchPointerEvent_(e);
  }

  private onPointerLeave_(e: PointerEvent) {
    if (e.pointerType !== 'mouse' || !this.isActivePointer_(e)) {
      return;
    }
    this.onPointerUpOrCancel_(new PointerEvent('pointerup', e));
  }

  private onPointerUpOrCancel_(e: PointerEvent) {
    if (!this.isActivePointer_(e)) {
      return;
    }
    this.activePointer_ = null;
    if (!this.pointerGesture_) {
      this.dispatchPointerEvent_(e);
      // If the stroke was not cancelled (type === pointercanel),
      // notify about mutation and record metrics.
      if (e.type === 'pointerup') {
        this.dispatchEvent(new CustomEvent('stroke-added'));
        if (e.pointerType === 'mouse') {
          record(UserAction.ANNOTATE_STROKE_DEVICE_MOUSE);
        } else if (e.pointerType === 'pen') {
          record(UserAction.ANNOTATE_STROKE_DEVICE_PEN);
        } else if (e.pointerType === 'touch') {
          record(UserAction.ANNOTATE_STROKE_DEVICE_TOUCH);
        }
        if (this.tool_!.tool === 'eraser') {
          record(UserAction.ANNOTATE_STROKE_TOOL_ERASER);
        } else if (this.tool_!.tool === 'pen') {
          record(UserAction.ANNOTATE_STROKE_TOOL_PEN);
        } else if (this.tool_!.tool === 'highlighter') {
          record(UserAction.ANNOTATE_STROKE_TOOL_HIGHLIGHTER);
        }
      }
    }
    this.pointerGesture_ = false;
  }

  private onPointerMove_(e: PointerEvent) {
    if (!this.isActivePointer_(e) || this.pointerGesture_) {
      return;
    }

    let events = e.getCoalescedEvents();
    if (events.length === 0) {
      events = [e];
    }
    for (const event of events) {
      this.dispatchPointerEvent_(event);
    }
  }

  /**
   * Begins annotation mode with the document represented by `data`.
   * When the return value resolves the Ink component will be ready
   * to render immediately.
   *
   * @param fileName The name of the PDF file.
   * @param data The contents of the PDF document.
   */
  async load(fileName: string, data: ArrayBuffer): Promise<void> {
    this.fileName_ = fileName;
    this.state_ = State.LOADING;

    const frame = this.shadowRoot!.querySelector<HTMLIFrameElement>('#frame')!;
    frame.src = 'ink/index.html';
    await new Promise(resolve => frame.onload = resolve);
    this.ink_ = await frame.contentWindow!.initInk();
    this.ink_!.addUndoStateListener(
        e => this.dispatchEvent(
            new CustomEvent('undo-state-changed', {detail: e})));
    await this.ink_!.setPdf(data);
    this.state_ = State.ACTIVE;
    this.viewportChanged();
    // Wait for the next task to avoid a race where Ink drops the background
    // color.
    await new Promise(resolve => setTimeout(resolve));
    this.ink_!.setOutOfBoundsColor(BACKGROUND_COLOR);
    const spacing = PAGE_SHADOW.top + PAGE_SHADOW.bottom;
    this.ink_!.setPageSpacing(spacing);
    this.style.visibility = 'visible';
  }

  viewportChanged() {
    if (this.state_ !== State.ACTIVE) {
      return;
    }
    const viewport = this.viewport!;
    const pos = viewport.position;
    const size = viewport.size;
    const zoom = viewport.getZoom();
    const documentWidth = viewport.getDocumentDimensions().width * zoom;
    // Adjust for page shadows.
    const y = pos.y - PAGE_SHADOW.top * zoom;
    let x = pos.x - PAGE_SHADOW.left * zoom;
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
    this.ink_!.setCamera(camera);
  }

  /** Undo the last edit action. */
  undo() {
    this.ink_!.undo();
  }

  /** Redo the last undone edit action. */
  redo() {
    this.ink_!.redo();
  }

  /**
   * @return The serialized PDF document including any annotations that were
   *     made.
   */
  async saveDocument(): Promise<{fileName: string, dataToSave: ArrayBuffer}> {
    if (this.state_ === State.ACTIVE) {
      const pdf = await this.ink_!.getPdfDestructive();
      this.buffer_ = await pdf.buffer;
      this.state_ = State.IDLE;
    }
    return {
      fileName: this.fileName_!,
      dataToSave: this.buffer_!,
    };
  }

  private updateShadow_(zoom: number) {
    const boxWidth = (50 * zoom) | 0;
    const shadowWidth = (8 * zoom) | 0;
    const width = boxWidth + shadowWidth * 2 + 2;
    const boxOffset = (width - boxWidth) / 2;

    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = width;

    const ctx = canvas.getContext('2d')!;
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

    this.ink_!.setBorderImage(canvas.toDataURL());
  }

  getInkApiForTesting(): InkApi {
    return this.ink_!;
  }

  getPenModeForTesting(): boolean {
    return this.penMode_;
  }
}

declare global {
  interface Window {
    initInk(): Promise<InkApi>;
  }

  interface HTMLElementTagNameMap {
    'viewer-ink-host': ViewerInkHostElement;
  }
}

customElements.define(ViewerInkHostElement.is, ViewerInkHostElement);
