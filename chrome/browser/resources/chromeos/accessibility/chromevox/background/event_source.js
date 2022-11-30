// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tracks event sources.
 */
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {EventSourceType} from '../common/event_source_type.js';

export const EventSourceState = {};

/**
 * Sets the current event source.
 * @param {EventSourceType} source
 */
EventSourceState.set = function(source) {
  EventSourceState.current_ = source;
};

/**
 * Gets the current event source.
 * @return {EventSourceType}
 */
EventSourceState.get = function() {
  return EventSourceState.current_;
};

/** @private {EventSourceType} */
EventSourceState.current_ =
    chrome.accessibilityPrivate.IS_DEFAULT_EVENT_SOURCE_TOUCH ?
    EventSourceType.TOUCH_GESTURE :
    EventSourceType.NONE;

BridgeHelper.registerHandler(
    BridgeConstants.EventSourceState.TARGET,
    BridgeConstants.EventSourceState.Action.GET, () => EventSourceState.get());
