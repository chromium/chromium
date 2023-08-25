// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RequestHandler} from './post_message_api_request_handler.js';

/**
 * @fileoverview Minimal TypeScript definitions to satisfy cases where
 * post_message_api_server.js is used from TypeScript files.
 */

/**
 * Class that provides the functionality for talking to a client
 * over the PostMessageAPI.  This should be subclassed and the subclass should
 * provide supported methods.
 */
export class PostMessageApiServer extends RequestHandler {
  constructor(
      clientElement: Element, targetUrl: string,
      messageOriginUrlFilter: string);

  /** Send initialization message to client element. */
  initialize(): void;

  /**
   *  Virtual method to be overridden by implementations of this class to notify
   * them that we were unable to initialize communication channel with the
   * `this.clientElement()`.
   */
  onInitializationError(origin: string): void;

  /**
   * Virtual method to be overridden by implementation of this class to notify
   * them that communication has successfully been initialized with the client
   * element.
   */
  onInitializationComplete(): void;
}
