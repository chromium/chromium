// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Registers onmessage event handler.
 * @param {MessageEvent} event Incoming message event.
 */
self.onmessage = function (event) {
  let data = /** @type {string} */ (event.data);

  console.debug('Message received from main script: ', data);

  self.postMessage(data);
};
