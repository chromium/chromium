// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A very basic worker to allow round-tripping and testing that
 * we're creating a valid worker.
 *
 * @nocompile
 * Disable closure compilation, as closure thinks this is a window, and
 * complains about Window.onmessage not existing.
 *
 */

self.onmessage = function(event) {
  const data = /** @type {string} */ (event.data);

  console.debug('Message received from main script: ', data);

  self.postMessage(data);
};
