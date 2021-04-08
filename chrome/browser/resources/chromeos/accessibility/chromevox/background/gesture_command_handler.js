// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles gesture-based commands.
 */

goog.provide('GestureCommandHandler');

goog.require('ChromeVoxState');
goog.require('CommandHandler');
goog.require('EventGenerator');
goog.require('EventSourceState');
goog.require('GestureCommandData');
goog.require('PointerHandler');

goog.scope(function() {
const RoleType = chrome.automation.RoleType;
const Gesture = chrome.accessibilityPrivate.Gesture;

/**
 * Global setting for the enabled state of this handler.
 * @param {boolean} state
 */
GestureCommandHandler.setEnabled = function(state) {
  GestureCommandHandler.enabled_ = state;
};

/**
 * Global setting for the enabled state of this handler.
 * @return {boolean}
 */
GestureCommandHandler.getEnabled = function() {
  return GestureCommandHandler.enabled_;
};

/**
 * Handles accessibility gestures from the touch screen.
 * @param {string} gesture The gesture to handle, based on the
 *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
 * @private
 */
GestureCommandHandler.onAccessibilityGesture_ = function(gesture, x, y) {
  if (!GestureCommandHandler.enabled_) {
    return;
  }

  EventSourceState.set(EventSourceType.TOUCH_GESTURE);

  const chromeVoxState = ChromeVoxState.instance;
  const monitor = chromeVoxState ? chromeVoxState.getUserActionMonitor() : null;
  if (gesture !== Gesture.SWIPE_LEFT2 && monitor &&
      !monitor.onGesture(gesture)) {
    // UserActionMonitor returns true if this gesture should propagate.
    // Prevent this gesture from propagating if it returns false.
    // Always allow SWIPE_LEFT2 to propagate, since it simulates the escape key.
    return;
  }

  if (gesture === Gesture.TOUCH_EXPLORE) {
    GestureCommandHandler.pointerHandler_.onTouchMove(x, y);
    return;
  }

  const commandData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
  if (!commandData) {
    return;
  }

  Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);

  // Check first for an accelerator action.
  if (commandData.acceleratorAction) {
    chrome.accessibilityPrivate.performAcceleratorAction(
        commandData.acceleratorAction);
    return;
  }

  // Handle gestures mapped to keys. Global keys are handled in place of
  // commands, and menu key overrides are handled only in menus.
  let key;
  const range = ChromeVoxState.instance.currentRange;
  if (range && range.start && range.start.node) {
    let inMenu = false;
    let node = range.start.node;
    while (node) {
      if (AutomationPredicate.menuItem(node)) {
        inMenu = true;
        break;
      }
      node = node.parent;
    }

    if (commandData.menuKeyOverride && inMenu) {
      key = commandData.menuKeyOverride;
    }
  }

  if (!key) {
    key = commandData.globalKey;
  }

  if (key) {
    EventGenerator.sendKeyPress(key.keyCode, key.modifiers);
    return;
  }

  // Always try to recover the range to the previous hover target, if there's no
  // range.
  if (!ChromeVoxState.instance.currentRange) {
    const recoverTo = GestureCommandHandler.pointerHandler_
                          .lastValidNodeBeforePointerInvalidation;
    if (recoverTo) {
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(recoverTo));
    }
  }

  const command = commandData.command;
  if (command) {
    CommandHandler.onCommand(command);
  }
};

/** @private {boolean} */
GestureCommandHandler.enabled_ = true;

/** Performs global setup. @private */
GestureCommandHandler.init_ = function() {
  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      GestureCommandHandler.onAccessibilityGesture_);

  GestureCommandHandler.pointerHandler_ = new PointerHandler();
};

/**
 * The global granularity for gestures.
 * @type {GestureGranularity}
 */
GestureCommandHandler.granularity = GestureGranularity.LINE;

GestureCommandHandler.init_();
});  // goog.scope
