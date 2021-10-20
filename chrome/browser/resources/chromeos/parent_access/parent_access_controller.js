// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIServer} from 'chrome://resources/js/post_message_api_server.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

/**
 * Class that implements the Chrome side of the ParentAccess PostMessageAPI.
 */
export class ParentAccessController extends PostMessageAPIServer {
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
    this.parentAccessResultResolver_ = new PromiseResolver();
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
   *     result was received.
   */
  whenParentAccessResult() {
    return this.parentAccessResultResolver_.promise;
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
    this.parentAccessResultResolver_.resolve(parentAccessResultProto);
  }
}
