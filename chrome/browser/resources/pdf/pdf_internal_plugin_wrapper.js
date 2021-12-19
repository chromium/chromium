// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GestureDetector, PinchEventDetail} from './gesture_detector.js';

const channel = new MessageChannel();

const sizer = document.querySelector('#sizer');
const plugin =
    /** @type {!HTMLEmbedElement} */ (document.querySelector('embed'));

const srcUrl = new URL(plugin.getAttribute('src'));
let parentOrigin = srcUrl.origin;
if (parentOrigin === 'chrome-untrusted://print') {
  // Within Print Preview, the source origin differs from the parent origin.
  parentOrigin = 'chrome://print';
}

// Plugin-to-parent message handlers. All messages are passed through, but some
// messages may affect this frame, too.
let isFormFieldFocused = false;
plugin.addEventListener('message', e => {
  switch (e.data.type) {
    case 'formFocusChange':
      // TODO(crbug.com/1279516): Ideally, the plugin would just consume
      // interesting keyboard events first.
      isFormFieldFocused = /** @type {{focused:boolean}} */ (e.data).focused;
      break;
  }

  channel.port1.postMessage(e.data);
});

// Parent-to-plugin message handlers. Most messages are passed through, but some
// messages (with handlers that `return` immediately) are meant only for this
// frame, not the plugin.
channel.port1.onmessage = e => {
  switch (e.data.type) {
    case 'loadArray':
      if (plugin.src.startsWith('blob:')) {
        URL.revokeObjectURL(plugin.src);
      }
      plugin.src = URL.createObjectURL(new Blob([e.data.dataToLoad]));
      plugin.setAttribute('has-edits', '');
      return;

    case 'syncScrollToRemote':
      window.scrollTo(e.data.x, e.data.y);
      channel.port1.postMessage({
        type: 'ackScrollToRemote',
        x: window.scrollX,
        y: window.scrollY,
      });
      return;

    case 'updateSize':
      sizer.style.width = `${e.data.width}px`;
      sizer.style.height = `${e.data.height}px`;
      return;

    case 'viewport':
      // Snoop on "viewport" message to support real RTL scrolling in Print
      // Preview.
      // TODO(crbug.com/1158670): Support real RTL scrolling in the PDF viewer.
      if (parentOrigin === 'chrome://print' && e.data.layoutOptions) {
        switch (e.data.layoutOptions.direction) {
          case 1:
            document.dir = 'rtl';
            break;
          case 2:
            document.dir = 'ltr';
            break;
          default:
            document.dir = '';
            break;
        }
      }
      break;
  }

  plugin.postMessage(e.data);
};

// Entangle parent-child message channel.
window.parent.postMessage(
    {type: 'connect', token: srcUrl.href}, parentOrigin, [channel.port2]);

// Forward "scroll" events back to the parent frame's `Viewport`.
window.addEventListener('scroll', () => {
  channel.port1.postMessage({
    type: 'syncScrollFromRemote',
    x: window.scrollX,
    y: window.scrollY,
  });
});

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
  switch (e.key) {
    case ' ':
      // Preventing Space happens in the "keypress" event handler.
      break;
    case 'PageDown':
    case 'PageUp':
      // Always prevent PageDown/PageUp.
      e.preventDefault();
      break;

    case 'ArrowDown':
    case 'ArrowLeft':
    case 'ArrowRight':
    case 'ArrowUp':
      // Don't prevent arrow navigation in form fields, or if modified.
      if (!isFormFieldFocused && !hasKeyModifiers(e)) {
        e.preventDefault();
      }
      break;

    case 'Escape':
    case 'Tab':
      // Print Preview is interested in Escape and Tab.
      break;

    default:
      if (e.ctrlKey || e.metaKey) {
        // Take over Ctrl+A, but not other shortcuts, such as zoom or print.
        if (e.key === 'a') {
          e.preventDefault();
        }
        break;
      }
      return;
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

// Suppress extra scroll by preventing the default "keypress" handler for Space.
// TODO(crbug.com/1279429): Ideally would prevent "keydown" instead, but this
// doesn't work when a plugin element has focus.
document.addEventListener('keypress', e => {
  switch (e.key) {
    case ' ':
      // Don't prevent Space in form fields.
      if (!isFormFieldFocused) {
        e.preventDefault();
      }
      break;
  }
});

// TODO(crbug.com/1252096): Load from chrome://resources/js/util.m.js instead.
/**
 * @param {!Event} e
 * @return {boolean}
 */
function hasKeyModifiers(e) {
  return !!(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey);
}
