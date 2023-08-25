// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal TypeScript definitions to satisfy cases where
 * post_message_api_request_handler.js is used from TypeScript files.
 */

/** Handler for requests that come to the window containing the contents. */
export class RequestHandler {
  constructor(
      clientElement: Element, messageOriginUrlFilter: string,
      targetUrl: string);

  /** Returns the target url that this request handler is communicating with. */
  targetUrl(): URL;

  /**
   * Returns the target window that this request handler is communicating with.
   */
  targetWindow(): Window;

  /**
   * The Window type element to which this request handler will listen for
   * messages.
   */
  clientElement(): Element;

  /** Determines if the specified origin matches the origin filter. */
  originMatchesFilter(origin: string): boolean;

  /**
   * Registers the specified method name with the specified
   * function.
   */
  registerMethod(methodName: string, method: (args: []) => void): void;

  /** Executes the method and returns the result. */
  handle(funcName: string, args: []): Promise<any>;

  /** Check whether the method can be handled by this handler. */
  canHandle(funcName: string): boolean;
}
