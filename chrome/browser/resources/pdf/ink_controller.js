// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {ContentController} from './controller.js';
import {Viewport} from './viewport.js';

// Note: Redefining this type here, to work around the fact that ink externs
// are only available on Chrome OS, so the targets that contain them cannot be
// built on other platforms.

/**
 * @typedef {{
 *   setAnnotationTool: function(AnnotationTool):void,
 *   viewportChanged: function():void,
 *   saveDocument: function():!Promise,
 *   undo: function():void,
 *   redo: function():void,
 *   load: function(string, !ArrayBuffer):!Promise,
 *   viewport: !Viewport,
 * }}
 */
let ViewerInkHostElement;

/**
 * Event types dispatched by the ink controller.
 * @enum {string}
 */
export const InkControllerEventType = {
  HAS_UNSAVED_CHANGES: 'InkControllerEventType.HAS_UNSAVED_CHANGES',
  LOADED: 'InkControllerEventType.LOADED',
  SET_ANNOTATION_UNDO_STATE: 'InkControllerEventType.SET_ANNOTATION_UNDO_STATE',
};

/**
 * Controller for annotation mode, on Chrome OS only. Fires the following events
 * from its event target:
 *   InkControllerEventType.HAS_UNSAVED_CHANGES: Fired to indicate there are ink
 *       annotations that have not been saved.
 *   InkControllerEventType.SET_ANNOTATION_UNDO_STATE: Contains information
 *       about whether undo or redo options are available.
 *  @implements {ContentController}
 */
export class InkController {
  constructor() {
    /** @private {!EventTarget} */
    this.eventTarget_ = new EventTarget();

    /** @private {boolean} */
    this.isActive_ = false;

    /** @private {!Viewport} */
    this.viewport_;

    /** @private {!HTMLDivElement} */
    this.contentElement_;

    /** @private {?ViewerInkHostElement} */
    this.inkHost_ = null;

    /** @private {?AnnotationTool} */
    this.tool_ = null;
  }

  /**
   * @param {!Viewport} viewport
   * @param {!HTMLDivElement} contentElement
   */
  init(viewport, contentElement) {
    this.viewport_ = viewport;
    this.contentElement_ = contentElement;
  }

  /**
   * @return {boolean}
   * @override
   */
  get isActive() {
    // Check whether `contentElement_` is defined as a signal that `init()` was
    // called.
    return !!this.contentElement_ && this.isActive_;
  }

  /**
   * @param {boolean} isActive
   * @override
   */
  set isActive(isActive) {
    this.isActive_ = isActive;
  }

  /**
   * @return {!EventTarget}
   * @override
   */
  getEventTarget() {
    return this.eventTarget_;
  }

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    this.tool_ = tool;
    if (this.inkHost_) {
      this.inkHost_.setAnnotationTool(tool);
    }
  }

  beforeZoom() {}

  afterZoom() {}

  print() {}

  /** @override */
  rotateClockwise() {
    // TODO(dstockwell): implement rotation
  }

  /** @override */
  rotateCounterclockwise() {
    // TODO(dstockwell): implement rotation
  }

  /** @override */
  setDisplayAnnotations(displayAnnotations) {}

  /** @override */
  setTwoUpView(enableTwoUpView) {
    // TODO(dstockwell): Implement two up view.
  }

  /** @override */
  viewportChanged() {
    this.inkHost_.viewportChanged();
  }

  /** @override */
  save(requestType) {
    return this.inkHost_.saveDocument();
  }

  /** @override */
  saveAttachment(index) {}

  /** @override */
  undo() {
    this.inkHost_.undo();
  }

  /** @override */
  redo() {
    this.inkHost_.redo();
  }

  /** @override */
  load(filename, data) {
    if (!this.inkHost_) {
      const inkHost = document.createElement('viewer-ink-host');
      this.contentElement_.appendChild(inkHost);
      this.inkHost_ = /** @type {!ViewerInkHostElement} */ (inkHost);
      this.inkHost_.viewport = this.viewport_;
      inkHost.addEventListener('stroke-added', e => {
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

  /** @override */
  unload() {
    this.inkHost_.remove();
    this.inkHost_ = null;
    this.isActive = false;
  }
}

addSingletonGetter(InkController);
