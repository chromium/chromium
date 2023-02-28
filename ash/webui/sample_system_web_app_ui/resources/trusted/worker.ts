// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A shared worker, with 2 basic features to show background processing
// interacting with foreground pages.
// 1) Marked with 'doubler' in e.data[0], this sets the doubler variable to a
// new value. It then broadcasts the new value to all attached pages. This is
// sent by the background page.
//
// 2) The default action saves the port away for later. It then just multiplies
// the 2 values in e.data with the doubler, and returns the value to the caller.
// This is meant to be called by the foreground page.

// Add an empty export to tell TypeScript this is a module so we can re-declare
// `self` below.
export {};
// Re-declare self as a SharedWorkerGlobalScope so we can use shared worker
// functions.
declare const self: SharedWorkerGlobalScope;

let doubler = 2;
const connectedPagePorts = new Set<MessagePort>();
self.onconnect = (event) => {
  const port = event.ports[0]!;
  port.onmessage = function(e) {
    if (e.data[0] == 'doubler') {
      doubler = e.data[1];
      port.postMessage(e.data[1]);
      connectedPagePorts.forEach(foregroundPort => {
        foregroundPort.postMessage(
            ['New additional value: ' + doubler, doubler]);
      });
    } else {
      connectedPagePorts.add(port);
      const myWorkerResult =
          ['Result: ' + (e.data[0] * e.data[1] * doubler), doubler];

      port.postMessage(myWorkerResult);
    }
  };
};
