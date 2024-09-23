// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotatorBrowserProxyImpl, browserProxy} from './annotator_browser_proxy.js';
import {UntrustedAnnotatorPageCallbackRouter} from './ash/webui/annotator/mojom/untrusted_annotator.mojom-webui.js';

/**
 * Returns the projector app element inside this current DOM.
 * @return {projectorApp.AnnotatorApi}
 */
function getAnnotatorElement() {
  return /** @type {projectorApp.AnnotatorApi} */ (
      document.querySelector('projector-ink-canvas-wrapper'));
}

/* @type {AnnotatorPageCallbackRouter} */
let annotatorPageRouter = null;

const observer = new MutationObserver(() => {
  if (getAnnotatorElement()) {
    if (annotatorPageRouter) {
      // We have already registered. Therefore, return early.
      return;
    }

    annotatorPageRouter = browserProxy.getAnnotatorCallbackRouter();

    // Register for callbacks from the browser process.
    annotatorPageRouter.undo.addListener(() => {
      try {
        getAnnotatorElement().undo();
      } catch (error) {
        console.error('AnnotatorToolErrorType.UNDO_ERROR', error);
      }
    });
    annotatorPageRouter.redo.addListener(() => {
      try {
        getAnnotatorElement().redo();
      } catch (error) {
        console.error('AnnotatorToolErrorType.REDO_ERROR', error);
      }
    });
    annotatorPageRouter.clear.addListener(() => {
      try {
        getAnnotatorElement().clear();
      } catch (error) {
        console.error('AnnotatorToolErrorType.CLEAR_ERROR', error);
      }
    });
    annotatorPageRouter.setTool.addListener((tool) => {
      try {
        getAnnotatorElement().setTool(tool);
      } catch (error) {
        console.error('AnnotatorToolErrorType.SET_TOOL_ERROR', error);
      }
    });

    // Pass calls to the browser process.
    const elem = getAnnotatorElement();
    elem.addUndoRedoListener((undoAvailable, redoAvailable) => {
      browserProxy.onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable);
    });
    elem.addCanvasInitializationCallback((success) => {
      browserProxy.onCanvasInitialized(success);
    });
  }
});

observer.observe(document, {childList: true, subtree: true});
