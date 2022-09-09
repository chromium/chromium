// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper class for making blocking requests that are resolved
 * elsewhere in the DOM.
 */
export class BlockingRequestManager {
  private makeRequest_: () => void;
  private callback_: (() => void)|null;

  /**
   * @param makeRequest Function to initiate flow for request. If
   *     no function is provided, it defaults to this.resolve, i.e. it
   *     immediately resolves all requests.
   */
  constructor(makeRequest?: () => void) {
    this.makeRequest_ = makeRequest || this.resolve;
    /**
     * Callback Provided in requests and called when the request is resolved.
     */
    this.callback_ = null;
  }

  /**
   * Make a blocking request.
   * @param callback Function to be called if/when the request issuccessfully
   *     resolved.
   */
  request(callback: () => void) {
    this.callback_ = callback;
    this.makeRequest_();
  }

  /** Called if/when request is resolved successfully. */
  resolve() {
    this.callback_!();
  }
}
