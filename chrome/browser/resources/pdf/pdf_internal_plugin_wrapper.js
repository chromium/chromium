// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GestureDetector, PinchEventDetail} from './gesture_detector.js';

const channel = new MessageChannel();

const plugin =
    /** @type {!HTMLEmbedElement} */ (document.querySelector('embed'));
plugin.addEventListener('message', e => channel.port1.postMessage(e.data));
channel.port1.onmessage = e => {
  if (e.data.type === 'loadArray') {
    if (plugin.src.startsWith('blob:')) {
      URL.revokeObjectURL(plugin.src);
    }
    plugin.src = URL.createObjectURL(new Blob([e.data.dataToLoad]));
  } else {
    plugin.postMessage(e.data);
  }
};

const srcUrl = new URL(plugin.getAttribute('src'));
let parentOrigin = srcUrl.origin;
if (parentOrigin === 'chrome-untrusted://print') {
  // Within Print Preview, the source origin differs from the parent origin.
  parentOrigin = 'chrome://print';
}
window.parent.postMessage(
    {type: 'connect', token: srcUrl.href}, parentOrigin, [channel.port2]);

/**
 * Relays gesture events to the parent frame.
 * @param {!Event} e The gesture event.
 */
function relayGesture(e) {
  const gestureEvent = /** @type {!CustomEvent<!PinchEventDetail>} */ (e);
  channel.port1.postMessage({
    type: 'gesture',
    gesture: {
      type: gestureEvent.type,
      detail: gestureEvent.detail,
    },
  });
}

const gestureDetector = new GestureDetector(plugin);
for (const type of ['pinchstart', 'pinchupdate', 'pinchend']) {
  gestureDetector.getEventTarget().addEventListener(type, relayGesture);
}

document.addEventListener('keydown', e => {
  // Only forward potential shortcut keys.
  if (!e.ctrlKey && !e.metaKey && e.key !== ' ') {
    return;
  }

  // Take over Ctrl+A, but not other shortcuts, such as zoom or print.
  if (e.key === 'a') {
    e.preventDefault();
  }
  channel.port1.postMessage({
    type: 'sendKeyEvent',
    keyEvent: {
      keyCode: e.keyCode,
      code: e.code,
      key: e.key,
      shiftKey: e.shiftKey,
      ctrlKey: e.ctrlKey,
      altKey: e.altKey,
      metaKey: e.metaKey,
    },
  });
});
