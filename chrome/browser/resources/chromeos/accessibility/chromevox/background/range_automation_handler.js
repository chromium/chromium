// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from ChromeVox's current range.
 */
import {AutomationPredicate} from '../../common/automation_predicate.js';
import {AutomationUtil} from '../../common/automation_util.js';
import {constants} from '../../common/constants.js';
import {CursorRange} from '../../common/cursors/range.js';
import {ChromeVoxEvent, CustomAutomationEvent} from '../common/custom_automation_event.js';
import {Msgs} from '../common/msgs.js';

import {BaseAutomationHandler} from './base_automation_handler.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from './chromevox_range.js';
import {DesktopAutomationHandler} from './desktop_automation_handler.js';
import {FocusBounds} from './focus_bounds.js';
import {Output} from './output/output.js';
import {OutputCustomEvent} from './output/output_types.js';

const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/** @implements {ChromeVoxRangeObserver} */
export class RangeAutomationHandler extends BaseAutomationHandler {
  /** @private */
  constructor() {
    super(null);

    /** @private {AutomationNode} */
    this.lastAttributeTarget_;

    /** @private {Output} */
    this.lastAttributeOutput_;

    /** @private {number} */
    this.delayedAttributeOutputId_ = -1;

    ChromeVoxRange.addObserver(this);
  }

  static init() {
    if (RangeAutomationHandler.instance) {
      throw 'Error: Trying to create two copies of singleton RangeAutomationHandler';
    }
    RangeAutomationHandler.instance = new RangeAutomationHandler();
  }

  /**
   * @param {CursorRange} newRange
   * @param {boolean=} opt_fromEditing
   */
  onCurrentRangeChanged(newRange, opt_fromEditing) {
    if (this.node_) {
      this.removeAllListeners();
      this.node_ = null;
    }

    if (!newRange || !newRange.start.node || !newRange.end.node) {
      return;
    }

    this.node_ = AutomationUtil.getLeastCommonAncestor(
                     newRange.start.node, newRange.end.node) ||
        newRange.start.node;

    // Some re-targeting is needed for cases like tables.
    let retarget = this.node_;
    while (retarget && retarget !== retarget.root) {
      // Table headers require retargeting for events because they often have
      // event types we care about e.g. sort direction.
      if (retarget.role === RoleType.COLUMN_HEADER ||
          retarget.role === RoleType.ROW_HEADER) {
        this.node_ = retarget;
        break;
      }
      retarget = retarget.parent;
    }

    // TODO: some of the events mapped to onAriaAttributeChanged need to have
    // specific handlers that only output the specific attribute. There also
    // needs to be an audit of all attribute change events to ensure they get
    // outputted.
    this.addListener_(
        EventType.ARIA_ATTRIBUTE_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(
        EventType.AUTO_COMPLETE_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(
        EventType.IMAGE_ANNOTATION_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(EventType.NAME_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(
        EventType.DESCRIPTION_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(EventType.ROLE_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(EventType.AUTOCORRECTION_OCCURED, this.onEventIfInRange);
    this.addListener_(
        EventType.CHECKED_STATE_CHANGED, this.onCheckedStateChanged);
    this.addListener_(
        EventType.CHECKED_STATE_DESCRIPTION_CHANGED,
        this.onCheckedStateChanged);
    this.addListener_(EventType.COLLAPSED, this.onEventIfInRange);
    this.addListener_(EventType.EXPANDED, this.onEventIfInRange);
    this.addListener_(EventType.INVALID_STATUS_CHANGED, this.onEventIfInRange);
    this.addListener_(EventType.LOCATION_CHANGED, this.onLocationChanged);
    this.addListener_(
        EventType.RELATED_NODE_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(EventType.ROW_COLLAPSED, this.onEventIfInRange);
    this.addListener_(EventType.ROW_EXPANDED, this.onEventIfInRange);
    this.addListener_(EventType.STATE_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(EventType.SORT_CHANGED, this.onAriaAttributeChanged);
  }

  /** @param {!ChromeVoxEvent} evt */
  onEventIfInRange(evt) {
    if (BaseAutomationHandler.disallowEventFromAction(evt)) {
      return;
    }

    const prev = ChromeVoxRange.current;
    if (!prev) {
      return;
    }

    // TODO: we need more fine grained filters for attribute changes.
    if (prev.contentEquals(CursorRange.fromNode(evt.target)) ||
        evt.target.state.focused) {
      const prevTarget = this.lastAttributeTarget_;

      // Re-target to active descendant if it exists.
      const prevOutput = this.lastAttributeOutput_;
      this.lastAttributeTarget_ = evt.target.activeDescendant || evt.target;
      this.lastAttributeOutput_ = new Output().withRichSpeechAndBraille(
          CursorRange.fromNode(this.lastAttributeTarget_), prev,
          OutputCustomEvent.NAVIGATE);
      if (this.lastAttributeTarget_ === prevTarget && prevOutput &&
          prevOutput.equals(this.lastAttributeOutput_)) {
        return;
      }

      // If the target or an ancestor is controlled by another control, we may
      // want to delay the output.
      let maybeControlledBy = evt.target;
      while (maybeControlledBy) {
        if (maybeControlledBy.controlledBy &&
            maybeControlledBy.controlledBy.find(n => Boolean(n.autoComplete))) {
          clearTimeout(this.delayedAttributeOutputId_);
          this.delayedAttributeOutputId_ = setTimeout(() => {
            this.lastAttributeOutput_.go();
          }, DesktopAutomationHandler.ATTRIBUTE_DELAY_MS);
          return;
        }
        maybeControlledBy = maybeControlledBy.parent;
      }

      this.lastAttributeOutput_.go();
    }
  }

  /** @param {!ChromeVoxEvent} evt */
  onAriaAttributeChanged(evt) {
    // Don't report changes on editable nodes since they interfere with text
    // selection changes. Users can query via Search+k for the current state
    // of the text field (which would also report the entire value).
    if (evt.target.state[StateType.EDITABLE]) {
      return;
    }

    // Don't report changes in static text nodes which can be extremely noisy.
    if (evt.target.role === RoleType.STATIC_TEXT) {
      return;
    }

    // Report attribute changes for specific generated events.
    if (evt.type === chrome.automation.EventType.SORT_CHANGED) {
      let msgId;
      if (evt.target.sortDirection ===
          chrome.automation.SortDirectionType.ASCENDING) {
        msgId = 'sort_ascending';
      } else if (
          evt.target.sortDirection ===
          chrome.automation.SortDirectionType.DESCENDING) {
        msgId = 'sort_descending';
      }
      if (msgId) {
        new Output().withString(Msgs.getMsg(msgId)).go();
      }
      return;
    }

    // Only report attribute changes on some *Option roles if it is selected.
    if ((evt.target.role === RoleType.MENU_LIST_OPTION ||
         evt.target.role === RoleType.LIST_BOX_OPTION) &&
        !evt.target.selected) {
      return;
    }

    this.onEventIfInRange(evt);
  }

  /**
   * Provides all feedback once a checked state changed event fires.
   * @param {!ChromeVoxEvent} evt
   */
  onCheckedStateChanged(evt) {
    if (!AutomationPredicate.checkable(evt.target)) {
      return;
    }

    const event =
        new CustomAutomationEvent(EventType.CHECKED_STATE_CHANGED, evt.target, {
          eventFrom: evt.eventFrom,
          eventFromAction: evt.eventFromAction,
          intents: evt.intents,
        });
    this.onEventIfInRange(event);
  }

  /**
   * Updates the focus ring if the location of the current range, or
   * an descendant of the current range, changes.
   * @param {!ChromeVoxEvent} evt
   */
  onLocationChanged(evt) {
    const cur = ChromeVoxRange.current;
    if (!cur || !cur.isValid()) {
      if (FocusBounds.get().length) {
        FocusBounds.set([]);
      }
      return;
    }

    // Rather than trying to figure out if the current range falls somewhere
    // in |evt.target|, just update it if our cached bounds don't match.
    const oldFocusBounds = FocusBounds.get();
    const startRect = cur.start.node.location;
    const endRect = cur.end.node.location;
    const found =
        oldFocusBounds.some(rect => this.areRectsEqual_(rect, startRect)) &&
        oldFocusBounds.some(rect => this.areRectsEqual_(rect, endRect));
    if (found) {
      return;
    }

    new Output().withLocation(cur, null, evt.type).go();
  }

  /**
   * @param {!chrome.accessibilityPrivate.ScreenRect} rectA
   * @param {!chrome.accessibilityPrivate.ScreenRect} rectB
   * @return {boolean} Whether the rects are the same.
   * @private
   */
  areRectsEqual_(rectA, rectB) {
    return rectA.left === rectB.left && rectA.top === rectB.top &&
        rectA.width === rectB.width && rectA.height === rectB.height;
  }
}

/** @type {RangeAutomationHandler} */
RangeAutomationHandler.instance;
