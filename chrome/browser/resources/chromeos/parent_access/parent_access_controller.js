// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiServer} from 'chrome://resources/ash/common/post_message_api/post_message_api_server.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';

/**
 * Class that implements the Chrome side of the ParentAccess PostMessageAPI.
 */
export class ParentAccessController extends PostMessageApiServer {
  /*
   * @param {!Element} webviewElement The <webview> element to listen to as a
   *     client.
   * @param {string} targetURL The target URL to use for outgoing messages.
   *     This should be the same as the URL loaded in the webview.
   * @param {string} originURLPrefix The URL prefix to use to filter incoming
   *     messages via the postMessage API.
   * @param {!function(string)} onParentAccessResultFn Function to call when
   *     parent access is granted.
   * @param {!function(string)} onInitializationErrorFn Function to call there
   *     was an error initializing the controller.
   */
  constructor(webviewElement, targetURL, originURLPrefix) {
    super(webviewElement, targetURL, originURLPrefix);

    /** @private {!PromiseResolver} */
    this.parentAccessCallbackReceivedResolver_ = new PromiseResolver();
    /** @private {!PromiseResolver} */
    this.initializationErrorResolver_ = new PromiseResolver();

    this.registerMethod('onParentAccessResult', (param) => {
      this.parentAccessResult_(param[0]);
    });
  }

  /*
   * @return {!Promise<string>} A promise that rejects when there was an error
   *     initializing the PostMessageAPI connection.
   */
  whenInitializationError() {
    return this.initializationErrorResolver_.promise;
  }

  /** @override */
  onInitializationError(origin) {
    this.initializationErrorResolver_.reject(origin);
  }

  /*
   * @return {!Promise<string>} A promise that resolves when a parent access
   *     callback was received.
   */
  whenParentAccessCallbackReceived() {
    return this.parentAccessCallbackReceivedResolver_.promise;
  }

  /**
   * Signals to the owner that the parent access web widget completed.
   * @private
   * @param {string} parentAccessResultProto The result of the parent
   *     verification returned by the web widget. It is a base64 encoded
   *     serialized proto that contains the proof of verification that can be
   *     used by the handler.
   */
  parentAccessResult_(parentAccessResultProto) {
    this.parentAccessCallbackReceivedResolver_.resolve(parentAccessResultProto);
    // Resets resolver to wait for the next callback.
    this.parentAccessCallbackReceivedResolver_ = new PromiseResolver();
  }
}
