// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';
import {RequestHandler} from 'chrome://resources/ash/common/post_message_api/post_message_api_request_handler.js';

import {AnnotatorBrowserProxy, AnnotatorBrowserProxyImpl} from './annotator_browser_proxy.js';

const TARGET_URL = 'chrome-untrusted://projector-annotator/';

// A PostMessageAPIClient that sends messages to chrome-untrusted://projector.
export class UntrustedAnnotatorClient extends PostMessageAPIClient {
  /**
   * @param {!Window} targetWindow
   */
  constructor(targetWindow) {
    super(TARGET_URL, targetWindow);
  }

  /**
   * Notifies the Annotator tool to update the tool.
   * @param {!projectorApp.AnnotatorToolParams} tool
   * @return {Promise<boolean>}
   */
  setTool(tool) {
    return this.callApiFn('setTool', [tool]);
  }

  /**
   * Notifies the Annotator to undo the last stroke.
   * @return {Promise<boolean>}
   */
  undo() {
    return this.callApiFn('undo', []);
  }

  /**
   * Notifies the Annotator to redo the last stroke.
   * @return {Promise<boolean>}
   */
  redo() {
    return this.callApiFn('redo', []);
  }

  /**
   * Notifies the Annotator to clear the screen.
   * @return {Promise<boolean>}
   */
  clear() {
    return this.callApiFn('clear', []);
  }
}

/**
 * Class that implements the RequestHandler inside the Projector trusted scheme
 * for Annotator.
 */
class TrustedAnnotatorRequestHandler extends RequestHandler {
  /*
   * @param {!Element} iframeElement The <iframe> element to listen to as a
   *     client.
   * @param {AnnotatorBrowserProxy} browserProxy The browser proxy that will
   *     be used to handle the messages.
   */
  constructor(iframeElement, browserProxy) {
    super(iframeElement, TARGET_URL, TARGET_URL);
    this.browserProxy_ = browserProxy;

    this.registerMethod('onUndoRedoAvailabilityChanged', (values) => {
      if (!values || values.length != 2) {
        return;
      }
      return this.browserProxy_.onUndoRedoAvailabilityChanged(
          values[0], values[1]);
    });

    this.registerMethod('onCanvasInitialized', (values) => {
      if (!values || values.length != 1) {
        return;
      }
      return this.browserProxy_.onCanvasInitialized(values[0]);
    });
  }
}

/**
 * This is a class that is used to setup the duplex communication
 * channels between this origin, chrome://projector/* and the iframe embedded
 * inside the document.
 */
export class AnnotatorTrustedCommFactory {
  /**
   * Creates the instances of PostMessageAPIClient and RequestHandler if they
   * have not been created already.
   */
  static maybeCreateInstances() {
    if (AnnotatorTrustedCommFactory.client_ ||
        AnnotatorTrustedCommFactory.requestHandler_) {
      return;
    }

    const iframeElement = document.getElementsByTagName('iframe')[0];

    AnnotatorTrustedCommFactory.client_ =
        new UntrustedAnnotatorClient(iframeElement.contentWindow);

    AnnotatorTrustedCommFactory.requestHandler_ =
        new TrustedAnnotatorRequestHandler(
            iframeElement, AnnotatorBrowserProxyImpl.getInstance());
  }

  /**
   * In order to use this class, please do the following
   * (e.g. to set the tool do the following):
   * const success = await
   * AnnotatorTrustedCommFactory.getPostMessageAPIClient().setTool(tool);
   *
   * @return {!UntrustedAnnotatorClient}
   */
  static getPostMessageAPIClient() {
    // AnnotatorTrustedCommFactory.client_ should be available. However to be on
    // the cautious side create an instance here if getPostMessageAPIClient is
    // triggered before the page finishes loading.
    AnnotatorTrustedCommFactory.maybeCreateInstances();
    return AnnotatorTrustedCommFactory.client_;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Create instances of the singletons(PostMessageAPIClient and
  // RequestHandler) when the document has finished loading.
  AnnotatorTrustedCommFactory.maybeCreateInstances();
});
