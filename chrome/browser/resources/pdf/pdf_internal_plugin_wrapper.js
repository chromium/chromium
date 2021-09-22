// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GestureDetector} from './gesture_detector.js';

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
window.parent.postMessage(
    {type: 'connect', token: srcUrl.href}, srcUrl.origin, [channel.port2]);

const gestureDetector = new GestureDetector(plugin);
