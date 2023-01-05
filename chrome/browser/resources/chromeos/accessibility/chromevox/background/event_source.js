// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tracks event sources.
 */
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {EventSourceType} from '../common/event_source_type.js';

export class EventSourceState {
  constructor() {
    /** @private {!EventSourceType} */
    this.state_ = chrome.accessibilityPrivate.IS_DEFAULT_EVENT_SOURCE_TOUCH ?
        EventSourceType.TOUCH_GESTURE :
        EventSourceType.NONE;
  }

  static init() {
    EventSourceState.instance = new EventSourceState();

    BridgeHelper.registerHandler(
        BridgeConstants.EventSourceState.TARGET,
        BridgeConstants.EventSourceState.Action.GET,
        () => EventSourceState.get());
  }

  /** @param {!EventSourceType} source */
  static set(source) {
    EventSourceState.instance.state_ = source;
  }

  /** @return {!EventSourceType} */
  static get() {
    return EventSourceState.instance.state_;
  }
}
