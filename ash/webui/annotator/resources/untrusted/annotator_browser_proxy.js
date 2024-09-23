// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UntrustedAnnotatorPageCallbackRouter, UntrustedAnnotatorPageHandlerFactory, UntrustedAnnotatorPageHandlerRemote, UntrustedAnnotatorPageRemote} from './ash/webui/annotator/mojom/untrusted_annotator.mojom-webui.js';

/**
 * To use the annotator proxy, please import this module and call
 * AnnotatorBrowserProxyImpl.getInstance().*
 *
 * @interface
 */
export class AnnotatorBrowserProxy {
  /**
   * Annotator
   * Notifies the embedder content that undo/redo availability changed for
   * annotator.
   * @param {boolean} undoAvailable
   * @param {boolean} redoAvailable
   */
  onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable) {}

  /**
   * Annotator
   * Notifies the embedder content that the canvas has either succeeded or
   * failed to initialize.
   * @param {boolean} success
   */
  onCanvasInitialized(success) {}
}

/**
 * @implements {AnnotatorBrowserProxy}
 */
export class AnnotatorBrowserProxyImpl {
  constructor() {
    this.pageHandlerFactory = UntrustedAnnotatorPageHandlerFactory.getRemote();
    this.pageHandlerRemote = new UntrustedAnnotatorPageHandlerRemote();
    this.annotatorCallbackRouter = new UntrustedAnnotatorPageCallbackRouter();

    this.pageHandlerFactory.create(
        this.pageHandlerRemote.$.bindNewPipeAndPassReceiver(),
        this.annotatorCallbackRouter.$.bindNewPipeAndPassRemote());
  }

  getAnnotatorCallbackRouter() {
    return this.annotatorCallbackRouter;
  }

  /** @override */
  onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable) {
    this.pageHandlerRemote.onUndoRedoAvailabilityChanged(
        undoAvailable, redoAvailable);
  }

  /** @override */
  onCanvasInitialized(success) {
    this.pageHandlerRemote.onCanvasInitialized(success);
  }
}

/**
 * @type {AnnotatorBrowserProxyImpl}
 */
export const browserProxy = new AnnotatorBrowserProxyImpl();
