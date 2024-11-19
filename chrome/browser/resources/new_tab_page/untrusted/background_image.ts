// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The code in this file tracks the load time of the background
 * image in order to send that time to the main NTP frame for metrics logging.
 */

let loadTime: number;

function sendLoadTime(time: number) {
  window.parent.postMessage(
      {
        frameType: 'background-image',
        messageType: 'loaded',
        url: location.href,
        time: time,
      },
      'chrome://new-tab-page');
}

// The function is used in background_image.html.
// eslint-disable-next-line @typescript-eslint/no-unused-vars
function onImageLoad() {
  document.body.toggleAttribute('shown', true);
  loadTime = Date.now();
  sendLoadTime(loadTime);
}

// The NTP requests the load time as soon as it has installed the message
// listener. In case we have already sent the load time we re-send the load time
// so that the NTP has a chance to actually catch it.
window.addEventListener('message', ({data}) => {
  if (data === 'sendLoadTime' && loadTime) {
    sendLoadTime(loadTime);
  }
});
