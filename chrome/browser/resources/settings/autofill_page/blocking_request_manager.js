// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper class for making blocking requests that are resolved
 * elsewhere in the DOM.
 */
cr.define('settings', function() {
  class BlockingRequestManager {
    /**
     * @param {Function=} makeRequest Function to initiate flow for request. If
     *     no function is provided, it defaults to this.resolve, i.e. it
     *     immediately resolves all requests.
     */
    constructor(makeRequest) {
      this.makeRequest_ = makeRequest || this.resolve;
      /**
       * @private {Function} callback Provided in requests and called when the
       *     request is resolved.
       */
      this.callback_ = null;
    }

    /**
     * Make a blocking request.
     * @param {Function} callback Function to be called if/when the request is
     *     successfully resolved.
     */
    request(callback) {
      this.callback_ = callback;
      this.makeRequest_();
    }

    /** Called if/when request is resolved successfully. */
    resolve() {
      this.callback_();
    }
  }

  return {
    BlockingRequestManager: BlockingRequestManager,
  };
});
