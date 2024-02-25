// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal TypeScript definitions to satisfy cases where
 * post_message_api_client.js is used from TypeScript files.
 */

/**
 * Class that provides the functionality for talking to a PostMessageAPIServer
 * over the postMessage API.  This should be subclassed and the subclass should
 * expose methods that are implemented by the server. The following is an
 * example.
 * class FooClient extends PostMessageAPIClient {
 *  ...
 *   doFoo(args) {
 *    return this.callApiFn('foo', args);
 *   }
 * }
 *
 */
export class PostMessageApiClient {
  constructor(serverOriginUrlFilter: string, targetWindow: Window|null);

  /**
   * Virtual method called when the client is initialized and it knows the
   * server that it is communicating with. This method should be overwritten by
   * subclasses which would like to know when initialization is done.
   */
  onInitialized(): void;

  /**
   * Returns if the client's connection to its handler is initialized or not.
   */
  isInitialized(): boolean;

  /**
   * Converts a function call with arguments into a postMessage event
   * and sends it to the server via the postMessage API.
   */
  callApiFn(fn: string, args: any[]): Promise<any>;
}
