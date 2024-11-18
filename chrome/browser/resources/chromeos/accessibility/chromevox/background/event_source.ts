// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tracks event sources.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from '../common/bridge_constants.js';
import {EventSourceType} from '../common/event_source_type.js';

export class EventSource {
  private state_: EventSourceType;

  static instance: EventSource;

  private constructor() {
    this.state_ = chrome.accessibilityPrivate.IS_DEFAULT_EVENT_SOURCE_TOUCH ?
        EventSourceType.TOUCH_GESTURE :
        EventSourceType.NONE;
  }

  static init(): void {
    EventSource.instance = new EventSource();

    BridgeHelper.registerHandler(
        BridgeConstants.EventSource.TARGET,
        BridgeConstants.EventSource.Action.GET, () => EventSource.get());
  }

  static set(source: EventSourceType): void {
    EventSource.instance.state_ = source;
  }

  static get(): EventSourceType|undefined {
    return EventSource.instance.state_;
  }
}

TestImportManager.exportForTesting(EventSource);
