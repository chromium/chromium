// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Point} from './constants.js';
import {GestureDetector, PinchEventDetail} from './gesture_detector.js';
import {SwipeDetector, SwipeDirection} from './swipe_detector.js';
import {ViewportInterface, ViewportScroller} from './viewport_scroller.js';

interface InProcessPdfPluginElement extends HTMLEmbedElement {
  postMessage(message: any): void;
}

const channel = new MessageChannel();

const sizer = document.querySelector<HTMLElement>('#sizer')!;
const plugin = document.querySelector<InProcessPdfPluginElement>('embed')!;

const srcUrl = new URL(plugin.src);
let parentOrigin = srcUrl.origin;
if (parentOrigin === 'chrome-untrusted://print') {
  // Within Print Preview, the source origin differs from the parent origin.
  parentOrigin = 'chrome://print';
}

/**
 * {@link Viewport}-compatible wrapper around the window's scroll position
 * operations.
 */
class SimulatedViewport implements ViewportInterface {
  get position(): Point {
    return {x: window.scrollX, y: window.scrollY};
  }

  setPosition(point: Point): void {
    window.scrollTo(point.x, point.y);
  }
}
const viewportScroller =
    new ViewportScroller(new SimulatedViewport(), plugin, window);

// Plugin-to-parent message handlers. All messages are passed through, but some
// messages may affect this frame, too.
let isFormFieldFocused = false;
plugin.addEventListener('message', e => {
  const message = (e as MessageEvent).data;
  switch (message.type) {
    case 'formFocusChange':
      // TODO(crbug.com/1279516): Ideally, the plugin would just consume
      // interesting keyboard events first.
      isFormFieldFocused = (message as {focused: boolean}).focused;
      break;

    case 'setIsSelecting':
      viewportScroller.setEnableScrolling(
          (message as {isSelecting: boolean}).isSelecting);
      break;
  }

  channel.port1.postMessage(message);
});

// Parent-to-plugin message handlers. Most messages are passed through, but some
// messages (with handlers that `return` immediately) are meant only for this
// frame, not the plugin.
let isPresentationMode = false;
channel.port1.onmessage = e => {
  switch (e.data.type) {
    case 'loadArray':
      if (plugin.src.startsWith('blob:')) {
        URL.revokeObjectURL(plugin.src);
      }
      plugin.src = URL.createObjectURL(new Blob([e.data.dataToLoad]));
      plugin.setAttribute('has-edits', '');
      return;

    case 'setPresentationMode':
      isPresentationMode = e.data.enablePresentationMode;

      gestureDetector.setPresentationMode(isPresentationMode);
      swipeDetector.setPresentationMode(isPresentationMode);
      if (isPresentationMode) {
        document.documentElement.className = 'fullscreen';
      } else {
        document.documentElement.className = '';

        // Ensure that directional keys still work after exiting.
        plugin.focus();
      }
      break;

    case 'syncScrollToRemote':
      // TODO(crbug.com/1306236): Implement smooth scrolling correctly.
      window.scrollTo({
        left: e.data.x,
        top: e.data.y,
      });
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
 * @param e The gesture event.
 */
function relayGesture(e: Event): void {
  const gestureEvent = e as CustomEvent<PinchEventDetail>;
  channel.port1.postMessage({
    type: 'gesture',
    gesture: {
      type: gestureEvent.type,
      detail: gestureEvent.detail,
    },
  });
}

const gestureDetector = new GestureDetector(plugin);
for (const type of ['pinchstart', 'pinchupdate', 'pinchend', 'wheel']) {
  gestureDetector.getEventTarget().addEventListener(type, relayGesture);
}

/**
 * Relays swipe events to the parent frame.
 * @param e The swipe event.
 */
function relaySwipe(e: Event): void {
  const swipeEvent = e as CustomEvent<SwipeDirection>;
  channel.port1.postMessage({
    type: 'swipe',
    direction: swipeEvent.detail,
  });
}

const swipeDetector = new SwipeDetector(plugin);
swipeDetector.getEventTarget().addEventListener('swipe', relaySwipe);

document.addEventListener('keydown', e => {
  // Only forward potential shortcut keys.
  switch (e.key) {
    case ' ':
      // Preventing Space happens in the "keypress" event handler.
      break;
    case 'PageDown':
    case 'PageUp':
      // Prevent PageDown/PageUp when there are no modifier keys.
      if (!hasKeyModifiers(e)) {
        e.preventDefault();
        break;
      }
      return;

    case 'ArrowDown':
    case 'ArrowLeft':
    case 'ArrowRight':
    case 'ArrowUp':
      // Don't prevent arrow navigation in form fields, or if modified.
      if (!isFormFieldFocused && !hasKeyModifiers(e)) {
        e.preventDefault();
        break;
      }
      return;

    case 'Escape':
    case 'Tab':
      // Print Preview is interested in Escape and Tab.
      break;

    case '=':
    case '-':
    case '+':
      // Ignore zoom shortcuts in Presentation mode.
      if (isPresentationMode && hasCtrlModifier(e)) {
        e.preventDefault();
      }
      return;

    case 'a':
      // Take over Ctrl+A (but not other combinations like Ctrl-Shift-A).
      // Note that on macOS, "Ctrl" is Command.
      if (hasCtrlModifierOnly(e)) {
        e.preventDefault();
        break;
      }
      return;

    default:
      // Relay (but don't prevent) other shortcuts.
      if (hasCtrlModifier(e)) {
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

// TODO(crbug.com/1252096): Load from pdf_viewer_utils.js instead.
function hasCtrlModifier(e: KeyboardEvent): boolean {
  let hasModifier = e.ctrlKey;
  // <if expr="is_macosx">
  hasModifier = e.metaKey;  // AKA Command.
  // </if>
  return hasModifier;
}

// TODO(crbug.com/1252096): Load from pdf_viewer_utils.js instead.
function hasCtrlModifierOnly(e: KeyboardEvent): boolean {
  let metaModifier = e.metaKey;
  // <if expr="is_macosx">
  metaModifier = e.ctrlKey;
  // </if>
  return hasCtrlModifier(e) && !e.shiftKey && !e.altKey && !metaModifier;
}

// TODO(crbug.com/1252096): Load from chrome://resources/js/util.js instead.
function hasKeyModifiers(e: KeyboardEvent): boolean {
  return !!(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey);
}
