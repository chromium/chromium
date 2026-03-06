// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './annotator_browser_proxy.js';
import {UntrustedAnnotatorPageCallbackRouter} from './untrusted_annotator.mojom-webui.js';

interface AnnotatorToolParams {
  tool: string;
  color: string;
  size: number;
}

interface AnnotatorApi {
  undo(): void;
  redo(): void;
  clear(): void;
  setTool(tool: AnnotatorToolParams|null): void;
  addUndoRedoListener(
      callback: (undoAvailable: boolean, redoAvailable: boolean) => void): void;
  addCanvasInitializationCallback(callback: (success: boolean) => void): void;
}

/**
 * Returns the projector app element inside this current DOM.
 */
function getAnnotatorElement() {
  return document.querySelector('projector-ink-canvas-wrapper') as
      AnnotatorApi |
      null;
}

/* @type {AnnotatorPageCallbackRouter} */
let annotatorPageRouter: UntrustedAnnotatorPageCallbackRouter|null = null;

const observer = new MutationObserver(() => {
  const annotatorElement = getAnnotatorElement();
  if (annotatorElement) {
    if (annotatorPageRouter) {
      // We have already registered. Therefore, return early.
      return;
    }

    annotatorPageRouter = browserProxy.getAnnotatorCallbackRouter();

    // Register for callbacks from the browser process.
    annotatorPageRouter.undo.addListener(() => {
      try {
        annotatorElement.undo();
      } catch (error) {
        console.error('AnnotatorToolErrorType.UNDO_ERROR', error);
      }
    });
    annotatorPageRouter.redo.addListener(() => {
      try {
        annotatorElement.redo();
      } catch (error) {
        console.error('AnnotatorToolErrorType.REDO_ERROR', error);
      }
    });
    annotatorPageRouter.clear.addListener(() => {
      try {
        annotatorElement.clear();
      } catch (error) {
        console.error('AnnotatorToolErrorType.CLEAR_ERROR', error);
      }
    });
    annotatorPageRouter.setTool.addListener(
        (tool: AnnotatorToolParams|null) => {
          try {
            annotatorElement.setTool(tool);
          } catch (error) {
            console.error('AnnotatorToolErrorType.SET_TOOL_ERROR', error);
          }
        });

    annotatorElement.addUndoRedoListener((undoAvailable, redoAvailable) => {
      browserProxy.onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable);
    });
    annotatorElement.addCanvasInitializationCallback((success) => {
      browserProxy.onCanvasInitialized(success);
    });
  }
});

observer.observe(document, {childList: true, subtree: true});
