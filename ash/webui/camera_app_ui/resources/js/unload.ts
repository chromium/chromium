// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbacks: Array<(() => void)> = [];

function onWindowUnload() {
  for (const callback of callbacks) {
    callback();
  }
  window.removeEventListener('unload', onWindowUnload);
}

window.addEventListener('unload', onWindowUnload);

/**
 * Adds a callback into the callback list. It follows the FIFO order.
 */
export function addUnloadCallback(callback: () => void): void {
  callbacks.push(callback);
}
