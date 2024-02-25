// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiServer} from 'chrome://resources/ash/common/post_message_api/post_message_api_server.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';

/**
 * Class that implements the Chrome side of the ParentAccess PostMessageAPI.
 */
export class ParentAccessController extends PostMessageApiServer {
  private parentAccessCallbackReceivedResolver: PromiseResolver<string>;
  private initializationErrorResolver: PromiseResolver<string>;

  constructor(
      webviewElement: Element, targetURL: string, originURLPrefix: string) {
    super(webviewElement, targetURL, originURLPrefix);

    this.parentAccessCallbackReceivedResolver = new PromiseResolver();
    this.initializationErrorResolver = new PromiseResolver();

    this.registerMethod('onParentAccessResult', (param: string[]) => {
      this.parentAccessResult(param[0]);
    });
  }

  /*
   * Returns a promise that rejects when there was an error initializing the
   * PostMessageAPI connection.
   */
  whenInitializationError(): Promise<string> {
    return this.initializationErrorResolver.promise;
  }

  override onInitializationError(origin: string) {
    this.initializationErrorResolver.reject(origin);
  }

  whenParentAccessCallbackReceived(): Promise<string> {
    return this.parentAccessCallbackReceivedResolver.promise;
  }

  /**
   * Signals to the owner that the parent access web widget completed.
   * Takes the result of the parent verification returned by the web widget. It
   * is a base64 encoded serialized proto that contains the proof of
   * verification that can be used by the handler.
   */
  private parentAccessResult(parentAccessResultProto: string) {
    this.parentAccessCallbackReceivedResolver.resolve(parentAccessResultProto);
    // Resets resolver to wait for the next callback.
    this.parentAccessCallbackReceivedResolver = new PromiseResolver();
  }
}
