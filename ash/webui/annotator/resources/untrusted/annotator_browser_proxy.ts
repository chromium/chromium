// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UntrustedAnnotatorPageCallbackRouter, UntrustedAnnotatorPageHandlerFactory, UntrustedAnnotatorPageHandlerRemote} from './untrusted_annotator.mojom-webui.js';

/**
 * To use the annotator proxy, please import this module and call
 * AnnotatorBrowserProxyImpl.getInstance().*
 */
export interface AnnotatorBrowserProxy {
  /**
   * Annotator
   * Notifies the embedder content that undo/redo availability changed for
   * annotator.
   * @param undoAvailable
   * @param redoAvailable
   */
  onUndoRedoAvailabilityChanged(undoAvailable: boolean, redoAvailable: boolean):
      void;

  /**
   * Annotator
   * Notifies the embedder content that the canvas has either succeeded or
   * failed to initialize.
   * @param success
   */
  onCanvasInitialized(success: boolean): void;
}

export class AnnotatorBrowserProxyImpl implements AnnotatorBrowserProxy {
  private pageHandlerRemote: UntrustedAnnotatorPageHandlerRemote;
  private annotatorCallbackRouter: UntrustedAnnotatorPageCallbackRouter;

  constructor() {
    const pageHandlerFactory = UntrustedAnnotatorPageHandlerFactory.getRemote();
    this.pageHandlerRemote = new UntrustedAnnotatorPageHandlerRemote();
    this.annotatorCallbackRouter = new UntrustedAnnotatorPageCallbackRouter();

    pageHandlerFactory.create(
        this.pageHandlerRemote.$.bindNewPipeAndPassReceiver(),
        this.annotatorCallbackRouter.$.bindNewPipeAndPassRemote());
  }

  getAnnotatorCallbackRouter() {
    return this.annotatorCallbackRouter;
  }

  onUndoRedoAvailabilityChanged(
      undoAvailable: boolean, redoAvailable: boolean) {
    this.pageHandlerRemote.onUndoRedoAvailabilityChanged(
        undoAvailable, redoAvailable);
  }

  onCanvasInitialized(success: boolean) {
    this.pageHandlerRemote.onCanvasInitialized(success);
  }
}

/**
 * @type {AnnotatorBrowserProxyImpl}
 */
export const browserProxy = new AnnotatorBrowserProxyImpl();
