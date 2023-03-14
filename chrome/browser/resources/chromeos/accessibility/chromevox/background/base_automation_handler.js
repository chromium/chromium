// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Basic facillities to handle events from a single automation
 * node.
 */
import {CursorRange} from '../../common/cursors/range.js';
import {ChromeVoxEvent} from '../common/custom_automation_event.js';
import {EventSourceType} from '../common/event_source_type.js';

import {ChromeVoxRange} from './chromevox_range.js';
import {EventSource} from './event_source.js';
import {Output} from './output/output.js';

const ActionType = chrome.automation.ActionType;
const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;

export class BaseAutomationHandler {
  /** @param {?AutomationNode} node */
  constructor(node) {
    /** @type {?AutomationNode} */
    this.node_ = node;

    /**
     * @private {!Object<EventType, function(!AutomationEvent)>}
     */
    this.listeners_ = {};
  }

  /**
   * Adds an event listener to this handler.
   * @param {EventType} eventType
   * @param {!function(!AutomationEvent)} eventCallback
   * @protected
   */
  addListener_(eventType, eventCallback) {
    if (this.listeners_[eventType]) {
      throw 'Listener already added: ' + eventType;
    }

    // Note: Keeping this bind lets us keep the addListener_ callsites simpler.
    const listener = this.makeListener_(eventCallback.bind(this));
    this.node_.addEventListener(eventType, listener, true);
    this.listeners_[eventType] = listener;
  }

  /** Removes all listeners from this handler. */
  removeAllListeners() {
    for (const eventType in this.listeners_) {
      this.node_.removeEventListener(
          eventType, this.listeners_[eventType], true);
    }

    this.listeners_ = {};
  }

  /**
   * @return {!function(!AutomationEvent): void}
   * @private
   */
  makeListener_(callback) {
    return evt => {
      if (this.willHandleEvent_(evt)) {
        return;
      }
      callback(evt);
      this.didHandleEvent_(evt);
    };
  }

  /**
   * Called before the event |evt| is handled.
   * @return {boolean} True to skip processing this event.
   * @protected
   */
  willHandleEvent_(evt) {
    return false;
  }

  /**
   * Called after the event |evt| is handled.
   * @protected
   */
  didHandleEvent_(evt) {}

  /**
   * Provides all feedback once ChromeVox's focus changes.
   * @param {!ChromeVoxEvent} evt
   */
  onEventDefault(evt) {
    const node = evt.target;
    if (!node) {
      return;
    }

    // Decide whether to announce and sync this event.
    const prevRange = ChromeVoxRange.getCurrentRangeWithoutRecovery();
    if ((prevRange && !prevRange.requiresRecovery()) &&
        BaseAutomationHandler.disallowEventFromAction(evt)) {
      return;
    }

    ChromeVoxRange.set(CursorRange.fromNode(node));

    // Because Closure doesn't know this is non-null.
    if (!ChromeVoxRange.current) {
      return;
    }

    // Don't output if focused node hasn't changed. Allow focus announcements
    // when interacting via touch. Touch never sets focus without a double tap.
    if (prevRange && evt.type === 'focus' &&
        ChromeVoxRange.current.equalsWithoutRecovery(prevRange) &&
        EventSource.get() !== EventSourceType.TOUCH_GESTURE) {
      return;
    }

    const output = new Output();
    output.withRichSpeechAndBraille(
        ChromeVoxRange.current, prevRange, evt.type);
    output.go();
  }

  /**
   * Returns true if the event contains an action that should not be processed.
   * @param {!ChromeVoxEvent} evt
   * @return {boolean}
   */
  static disallowEventFromAction(evt) {
    return !BaseAutomationHandler.announceActions &&
        evt.eventFrom === 'action' &&
        evt.eventFromAction !== ActionType.DO_DEFAULT &&
        evt.eventFromAction !== ActionType.SHOW_CONTEXT_MENU;
  }
}

/**
 * Controls announcement of non-user-initiated events.
 * @public {boolean}
 */
BaseAutomationHandler.announceActions = false;
