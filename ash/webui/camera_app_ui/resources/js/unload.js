// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!Array<function(): void>}
 */
const callbacks = [];

const onWindowUnload = () => {
  for (const callback of callbacks) {
    callback();
  }
  window.removeEventListener('unload', onWindowUnload);
};

window.addEventListener('unload', onWindowUnload);

/**
 * Adds a callback into the callback list. It follows the FIFO order.
 * @param {function(): void} callback
 */
export function addUnloadCallback(callback) {
  callbacks.push(callback);
}
