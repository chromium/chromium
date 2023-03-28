// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from '//resources/ash/common/post_message_api/post_message_api_client.js';
import {RequestHandler} from '//resources/ash/common/post_message_api/post_message_api_request_handler.js';

const TARGET_URL = 'chrome://projector-annotator/';

/**
 * Returns the projector app element inside this current DOM.
 * @return {projectorApp.AnnotatorApi}
 */
function getAnnotatorElement() {
  return /** @type {projectorApp.AnnotatorApi} */ (
      document.querySelector('projector-ink-canvas-wrapper'));
}


// A client that sends messages to the chrome://projector embedder.
export class TrustedAnnotatorClient extends PostMessageAPIClient {
  /**
   * @param {!Window} parentWindow The embedder window from which requests
   *     come.
   */
  constructor(parentWindow) {
    super(TARGET_URL, parentWindow);
  }

  /**
   * Notifies the native ui that undo/redo has become available.
   * @param {boolean} undoAvailable
   * @param {boolean} redoAvailable
   * @return {Promise}
   */
  onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable) {
    return this.callApiFn(
        'onUndoRedoAvailabilityChanged', [undoAvailable, redoAvailable]);
  }

  /**
   * Notifies the native UI that the canvas has initialized.
   * @param {boolean} success
   * @return {Promise}
   */
  onCanvasInitialized(success) {
    return this.callApiFn('onCanvasInitialized', [success]);
  }
}

/**
 * Class that implements the RequestHandler inside the Projector untrusted
 * scheme for Annotator.
 */
export class UntrustedAnnotatorRequestHandler extends RequestHandler {
  /**
   * @param {!Window} parentWindow The embedder window from which requests
   *     come.
   */
  constructor(parentWindow) {
    super(null, TARGET_URL, TARGET_URL);
    this.targetWindow_ = parentWindow;

    this.registerMethod('setTool', (args) => {
      getAnnotatorElement().setTool(args[0]);
      return true;
    });

    this.registerMethod('undo', () => {
      getAnnotatorElement().undo();
      return true;
    });

    this.registerMethod('redo', () => {
      getAnnotatorElement().redo();
      return true;
    });

    this.registerMethod('clear', () => {
      getAnnotatorElement().clear();
      return true;
    });
  }

  /** @override */
  targetWindow() {
    return this.targetWindow_;
  }
}

/**
 * This is a class that is used to setup the duplex communication channels
 * between this origin, chrome-untrusted://projector/* and the embedder content.
 */
export class AnnotatorUntrustedCommFactory {
  /**
   * Creates the instances of PostMessageAPIClient and Requesthandler.
   */
  static maybeCreateInstances() {
    if (AnnotatorUntrustedCommFactory.client_ ||
        AnnotatorUntrustedCommFactory.requestHandler_) {
      return;
    }

    AnnotatorUntrustedCommFactory.client_ =
        new TrustedAnnotatorClient(window.parent);

    AnnotatorUntrustedCommFactory.requestHandler_ =
        new UntrustedAnnotatorRequestHandler(window.parent);
    const elem = getAnnotatorElement();
    elem.addUndoRedoListener((undoAvailable, redoAvailable) => {
      AnnotatorUntrustedCommFactory.client_.onUndoRedoAvailabilityChanged(
          undoAvailable, redoAvailable);
    });
    elem.addCanvasInitializationCallback((success) => {
      AnnotatorUntrustedCommFactory.client_.onCanvasInitialized(success);
    });
  }

  /**
   * In order to use this class, please do the following (e.g. To notify when
   * undo-redo becomes available):
   * AnnotatorUntrustedCommFactory.
   *     getPostMessageAPIClient().
   *     onUndoRedoAvailabilityChanged(true, true);
   * @return {!TrustedAnnotatorClient}
   */
  static getPostMessageAPIClient() {
    // AnnotatorUntrustedCommFactory.client_ should be available. However to be
    // on the cautious side create an instance here if getPostMessageAPIClient
    // is triggered before the page finishes loading.
    AnnotatorUntrustedCommFactory.maybeCreateInstances();
    return AnnotatorUntrustedCommFactory.client_;
  }
}

const observer = new MutationObserver(() => {
  if (getAnnotatorElement()) {
    // Create instances of the singletons(PostMessageAPIClient and
    // RequestHandler) when the annotator element has been added to DOM tree.
    AnnotatorUntrustedCommFactory.maybeCreateInstances();
  }
});

observer.observe(document, {childList: true, subtree: true});
