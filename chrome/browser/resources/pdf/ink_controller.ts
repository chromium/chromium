// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

import type {AnnotationTool} from './annotation_tool.js';
import type {SaveRequestType} from './constants.js';
import type {ContentController, SaveAttachmentMessageData} from './controller.js';
import type {ViewerInkHostElement} from './elements/viewer_ink_host.js';
import type {Viewport} from './viewport.js';

/** Event types dispatched by the ink controller. */
export enum InkControllerEventType {
  HAS_UNSAVED_CHANGES = 'InkControllerEventType.HAS_UNSAVED_CHANGES',
  LOADED = 'InkControllerEventType.LOADED',
  SET_ANNOTATION_UNDO_STATE =
      'InkControllerEventType.SET_ANNOTATION_UNDO_STATE',
}

/**
 * Controller for annotation mode, on Chrome OS only. Fires the following events
 * from its event target:
 *   InkControllerEventType.HAS_UNSAVED_CHANGES: Fired to indicate there are ink
 *       annotations that have not been saved.
 *   InkControllerEventType.SET_ANNOTATION_UNDO_STATE: Contains information
 *       about whether undo or redo options are available.
 */
export class InkController implements ContentController {
  private eventTarget_: EventTarget = new EventTarget();
  private isActive_: boolean = false;
  private viewport_: Viewport;
  private inkHost_: ViewerInkHostElement|null = null;
  private tool_: AnnotationTool|null = null;

  init(viewport: Viewport) {
    this.viewport_ = viewport;
  }

  get isActive(): boolean {
    // Check whether `viewport_` is defined as a signal that `init()` was
    // called.
    return !!this.viewport_ && this.isActive_;
  }

  set isActive(isActive: boolean) {
    this.isActive_ = isActive;
  }

  getEventTarget(): EventTarget {
    return this.eventTarget_;
  }

  setAnnotationTool(tool: AnnotationTool) {
    this.tool_ = tool;
    if (this.inkHost_) {
      this.inkHost_.setAnnotationTool(tool);
    }
  }

  beforeZoom() {}

  afterZoom() {}

  print() {}

  rotateClockwise() {
    // TODO(dstockwell): implement rotation
  }

  rotateCounterclockwise() {
    // TODO(dstockwell): implement rotation
  }

  setDisplayAnnotations(_displayAnnotations: boolean) {}

  setTwoUpView(_enableTwoUpView: boolean) {
    // TODO(dstockwell): Implement two up view.
  }

  viewportChanged() {
    this.inkHost_!.viewportChanged();
  }

  save(_requestType: SaveRequestType) {
    return this.inkHost_!.saveDocument();
  }

  saveAttachment(_index: number): Promise<SaveAttachmentMessageData> {
    assertNotReached();
  }

  undo() {
    this.inkHost_!.undo();
  }

  redo() {
    this.inkHost_!.redo();
  }

  load(filename: string, data: ArrayBuffer) {
    if (!this.inkHost_) {
      const inkHost = document.createElement('viewer-ink-host');
      this.viewport_.setContent(inkHost);
      this.inkHost_ = inkHost;
      this.inkHost_.viewport = this.viewport_;
      inkHost.addEventListener('stroke-added', _e => {
        this.eventTarget_.dispatchEvent(
            new CustomEvent(InkControllerEventType.HAS_UNSAVED_CHANGES));
      });
      inkHost.addEventListener('undo-state-changed', e => {
        this.eventTarget_.dispatchEvent(new CustomEvent(
            InkControllerEventType.SET_ANNOTATION_UNDO_STATE,
            {detail: e.detail}));
      });
      this.isActive = true;
    }
    return this.inkHost_.load(filename, data).then(() => {
      this.eventTarget_.dispatchEvent(
          new CustomEvent(InkControllerEventType.LOADED));
    });
  }

  unload() {
    this.inkHost_!.remove();
    this.inkHost_ = null;
    this.isActive = false;
  }

  static getInstance(): InkController {
    return instance || (instance = new InkController());
  }
}

let instance: InkController|null = null;
