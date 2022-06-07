// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles gesture-based commands.
 */
import {ChromeVoxState} from '/chromevox/background/chromevox_state.js';
import {EventSourceState} from '/chromevox/background/event_source.js';
import {GestureInterface} from '/chromevox/background/gesture_interface.js';
import {Output} from '/chromevox/background/output/output.js';
import {PointerHandler} from '/chromevox/background/pointer_handler.js';
import {UserActionMonitor} from '/chromevox/background/user_action_monitor.js';
import {EventSourceType} from '/chromevox/common/event_source_type.js';
import {GestureCommandData, GestureGranularity} from '/chromevox/common/gesture_command_data.js';
import {EventGenerator} from '/common/event_generator.js';

const RoleType = chrome.automation.RoleType;
const Gesture = chrome.accessibilityPrivate.Gesture;

export const GestureCommandHandler = {};

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

  const monitor = UserActionMonitor.instance;
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

  // Always try to recover the range to the previous valid target which may
  // have been invalidated by touch explore; this recovery omits touch explore
  // explicitly.
  ChromeVoxState.instance.restoreLastValidRangeIfNeeded();

  // Handle gestures mapped to keys. Global keys are handled in place of
  // commands, and menu key overrides are handled only in menus.
  let key;
  const range = ChromeVoxState.instance.currentRange;
  if (range && range.start && range.start.node) {
    let inMenu = false;
    let node = range.start.node;
    while (node) {
      if (AutomationPredicate.menuItem(node) ||
          (node.role === RoleType.POP_UP_BUTTON && node.state.expanded)) {
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

  const command = commandData.command;
  if (command) {
    CommandHandlerInterface.instance.onCommand(command);
  }
};

/** @private {boolean} */
GestureCommandHandler.enabled_ = true;

/** Performs global setup. @private */
GestureCommandHandler.init_ = function() {
  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      GestureCommandHandler.onAccessibilityGesture_);

  GestureCommandHandler.pointerHandler_ = new PointerHandler();

  GestureInterface.granularityGetter = () => GestureCommandHandler.granularity;
  GestureInterface.granularitySetter = granularity =>
      GestureCommandHandler.granularity = granularity;
};

/**
 * The global granularity for gestures.
 * @type {GestureGranularity}
 */
GestureCommandHandler.granularity = GestureGranularity.LINE;

GestureCommandHandler.init_();

BridgeHelper.registerHandler(
    BridgeConstants.GestureCommandHandler.TARGET,
    BridgeConstants.GestureCommandHandler.Action.SET_ENABLED,
    enabled => GestureCommandHandler.setEnabled(enabled));
