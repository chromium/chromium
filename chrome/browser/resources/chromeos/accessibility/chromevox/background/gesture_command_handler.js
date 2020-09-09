// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles gesture-based commands.
 */

goog.provide('GestureCommandHandler');

goog.require('CommandHandler');
goog.require('EventGenerator');
goog.require('EventSourceState');
goog.require('GestureCommandData');
goog.require('PointerHandler');

goog.scope(function() {
const RoleType = chrome.automation.RoleType;

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

  if (gesture == 'touchExplore') {
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
  if (ChromeVoxState.instance.currentRange) {
    const range = ChromeVoxState.instance.currentRange;
    if (commandData.menuKeyOverride && range.start && range.start.node &&
        ((range.start.node.role == RoleType.MENU_ITEM &&
          range.start.node.root.role == RoleType.DESKTOP) ||
         range.start.node.root.docUrl.indexOf(
             chrome.extension.getURL('chromevox/panel/panel.html')) == 0)) {
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
