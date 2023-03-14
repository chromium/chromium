// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

import {AnnotatorPageCallbackRouter, AnnotatorPageHandlerFactory, AnnotatorPageHandlerRemote, AnnotatorPageRemote} from './ash/webui/projector_app/mojom/annotator.mojom-webui.js';

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

  /**
   * Sends 'error' message to handler.
   * The Handler will log the message. If the error is not a recoverable error,
   * the handler closes the corresponding WebUI.
   * @param {!Array<string>} msg Error messages.
   */
  onError(msg) {}
}

/**
 * @implements {AnnotatorBrowserProxy}
 */
export class AnnotatorBrowserProxyImpl {
  constructor() {
    this.pageHandlerFactory = AnnotatorPageHandlerFactory.getRemote();
    this.pageHandlerRemote = new AnnotatorPageHandlerRemote();
    this.annotatorCallbackRouter = new AnnotatorPageCallbackRouter();

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

  /** @override */
  onError(msgs) {
    this.pageHandlerRemote.onError(msgs);
  }
}

addSingletonGetter(AnnotatorBrowserProxyImpl);