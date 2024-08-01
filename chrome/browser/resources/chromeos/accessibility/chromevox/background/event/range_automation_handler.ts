// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from ChromeVox's current range.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {CursorRange} from '/common/cursors/range.js';

import {ChromeVoxEvent, CustomAutomationEvent} from '../../common/custom_automation_event.js';
import {Msgs} from '../../common/msgs.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from '../chromevox_range.js';
import {FocusBounds} from '../focus_bounds.js';
import {Output} from '../output/output.js';
import {OutputCustomEvent} from '../output/output_types.js';

import {BaseAutomationHandler} from './base_automation_handler.js';

type ActionType = chrome.automation.ActionType;
type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
type Rect = chrome.automation.Rect;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

export class RangeAutomationHandler extends BaseAutomationHandler
    implements ChromeVoxRangeObserver {
  private lastAttributeTarget_?: AutomationNode;
  private lastAttributeOutput_?: Output;
  private delayedAttributeOutputId_ = -1;

  private static instance: RangeAutomationHandler;

  private constructor() {
    super();
    ChromeVoxRange.addObserver(this);
  }

  static init(): void {
    if (RangeAutomationHandler.instance) {
      throw new Error(
        'Trying to create two copies of singleton RangeAutomationHandler');
    }
    RangeAutomationHandler.instance = new RangeAutomationHandler();
  }

  onCurrentRangeChanged(newRange: CursorRange, _fromEditing?: boolean): void {
    if (this.node_) {
      this.removeAllListeners();
      this.node_ = undefined;
    }

    if (!newRange || !newRange.start.node || !newRange.end.node) {
      return;
    }

    this.node_ = AutomationUtil.getLeastCommonAncestor(
                     newRange.start.node, newRange.end.node) ||
        newRange.start.node;

    // Some re-targeting is needed for cases like tables.
    let retarget: AutomationNode | undefined = this.node_;
    while (retarget && retarget !== retarget.root) {
      // Table headers require retargeting for events because they often have
      // event types we care about e.g. sort direction.
      if (AutomationPredicate.tableHeader(retarget)) {
        this.node_ = retarget;
        break;
      }
      retarget = retarget.parent;
    }

    // TODO: some of the events mapped to onAttributeChanged need to have
    // specific handlers that only output the specific attribute. There also
    // needs to be an audit of all attribute change events to ensure they get
    // outputted.
    // TODO(crbug.com/1464633) Fully remove ARIA_ATTRIBUTE_CHANGED_DEPRECATED
    // starting in 122, because although it was removed in 118, it is still
    // present in earlier versions of LaCros.
    this.addListener_(
        EventType.ARIA_ATTRIBUTE_CHANGED_DEPRECATED, this.onAttributeChanged);
    this.addListener_(EventType.AUTO_COMPLETE_CHANGED, this.onAttributeChanged);
    this.addListener_(
        EventType.IMAGE_ANNOTATION_CHANGED, this.onAttributeChanged);
    this.addListener_(EventType.NAME_CHANGED, this.onAttributeChanged);
    this.addListener_(EventType.DESCRIPTION_CHANGED, this.onAttributeChanged);
    this.addListener_(EventType.ROLE_CHANGED, this.onAttributeChanged);
    this.addListener_(EventType.AUTOCORRECTION_OCCURED, this.onEventIfInRange);
    this.addListener_(
        EventType.CHECKED_STATE_CHANGED, this.onCheckedStateChanged);
    this.addListener_(
        EventType.CHECKED_STATE_DESCRIPTION_CHANGED,
        this.onCheckedStateChanged);
    this.addListener_(EventType.COLLAPSED, this.onEventIfInRange);
    this.addListener_(EventType.CONTROLS_CHANGED, this.onControlsChanged);
    this.addListener_(EventType.EXPANDED, this.onEventIfInRange);
    this.addListener_(EventType.IMAGE_FRAME_UPDATED, this.onImageFrameUpdated_);
    this.addListener_(EventType.INVALID_STATUS_CHANGED, this.onEventIfInRange);
    this.addListener_(EventType.LOCATION_CHANGED, this.onLocationChanged);
    this.addListener_(EventType.RELATED_NODE_CHANGED, this.onAttributeChanged);
    this.addListener_(EventType.ROW_COLLAPSED, this.onEventIfInRange);
    this.addListener_(EventType.ROW_EXPANDED, this.onEventIfInRange);
    this.addListener_(EventType.STATE_CHANGED, this.onAttributeChanged);
    this.addListener_(EventType.SORT_CHANGED, this.onAttributeChanged);
  }

  onEventIfInRange(evt: ChromeVoxEvent): void {
    if (BaseAutomationHandler.disallowEventFromAction(evt)) {
      return;
    }

    const prev = ChromeVoxRange.current;
    if (!prev) {
      return;
    }

    // TODO: we need more fine grained filters for attribute changes.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (prev.contentEquals(CursorRange.fromNode(evt.target)) ||
        evt.target.state![StateType.FOCUSED]) {
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
      let maybeControlledBy: AutomationNode | undefined = evt.target;
      while (maybeControlledBy) {
        if (maybeControlledBy.controlledBy &&
            maybeControlledBy.controlledBy.find(n => Boolean(n.autoComplete))) {
          clearTimeout(this.delayedAttributeOutputId_);
          this.delayedAttributeOutputId_ = setTimeout(
            () => this.lastAttributeOutput_!.go(), ATTRIBUTE_DELAY_MS);
          return;
        }
        maybeControlledBy = maybeControlledBy.parent;
      }

      this.lastAttributeOutput_.go();
    }
  }

  onAttributeChanged(evt: ChromeVoxEvent): void {
    // Don't report changes on editable nodes since they interfere with text
    // selection changes. Users can query via Search+k for the current state
    // of the text field (which would also report the entire value).
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (evt.target.state![StateType.EDITABLE]) {
      return;
    }

    // Don't report changes in static text nodes which can be extremely noisy.
    if (evt.target.role === RoleType.STATIC_TEXT) {
      return;
    }

    // To avoid output of stale information, don't report changes in IME
    // candidates. IME candidate output is handled during selection events.
    if (evt.target.role === RoleType.IME_CANDIDATE) {
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
    if (AutomationPredicate.listOption(evt.target) && !evt.target.selected) {
      return;
    }

    this.onEventIfInRange(evt);
  }

  /** Provides all feedback once a checked state changed event fires. */
  onCheckedStateChanged(evt: ChromeVoxEvent): void {
    if (!AutomationPredicate.checkable(evt.target)) {
      return;
    }

    const event =
        new CustomAutomationEvent(EventType.CHECKED_STATE_CHANGED, evt.target, {
          eventFrom: evt.eventFrom,
          eventFromAction:
              (evt as CustomAutomationEvent).eventFromAction as ActionType,
          intents: evt.intents,
        });
    this.onEventIfInRange(event);
  }

  onControlsChanged(event: ChromeVoxEvent): void {
    if (event.target.role === RoleType.TAB) {
      new Output()
          .withSpeech(CursorRange.fromNode(event.target), undefined, event.type)
          .go();
    }
  }

  /**
   * Updates the focus ring if the location of the current range, or
   * an descendant of the current range, changes.
   */
  onLocationChanged(evt: ChromeVoxEvent): void {
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
    let startRect = cur.start.node.location;
    let endRect = cur.end.node.location;
    if (cur.start.node.activeDescendant) {
      startRect = cur.start.node.activeDescendant.location;
    }
    if (cur.end.node.activeDescendant) {
      endRect = cur.end.node.activeDescendant.location;
    }
    const found =
        oldFocusBounds.some(
            (rect: Rect) => this.areRectsEqual_(rect, startRect)) &&
        oldFocusBounds.some(
            (rect: Rect) => this.areRectsEqual_(rect, endRect));
    if (found) {
      return;
    }

    // Currently only considers if there's an active descendant on the
    // start node.
    const activeDescendant = cur.start.node.activeDescendant;
    if (activeDescendant) {
      new Output()
          .withLocation(
              CursorRange.fromNode(activeDescendant), undefined, evt.type)
          .go();
    } else {
      new Output().withLocation(cur, undefined, evt.type).go();
    }
  }

  /** Called when an image frame is received on a node. */
  private onImageFrameUpdated_(evt: ChromeVoxEvent): void {
    const target = evt.target;
    if (target.imageDataUrl) {
      ChromeVox.braille.writeRawImage(target.imageDataUrl);
    }
  }

  private areRectsEqual_(rectA: Rect, rectB: Rect): boolean {
    return rectA.left === rectB.left && rectA.top === rectB.top &&
        rectA.width === rectB.width && rectA.height === rectB.height;
  }
}

// Local to module.

/**
 * Time to wait before announcing attribute changes that are otherwise too
 * disruptive.
 */
const ATTRIBUTE_DELAY_MS = 1500;
