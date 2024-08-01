// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation events on the currently focused node.
 */
import {AsyncUtil} from '/common/async_util.js';
import {AutomationPredicate} from '/common/automation_predicate.js';
import {CursorRange} from '/common/cursors/range.js';

import {ChromeVoxEvent} from '../../common/custom_automation_event.js';
import {QueueMode, TtsSpeechProperties} from '../../common/tts_types.js';
import {ChromeVoxRange} from '../chromevox_range.js';
import {Output} from '../output/output.js';
import {OutputCustomEvent} from '../output/output_types.js';

import {BaseAutomationHandler} from './base_automation_handler.js';

type AutomationEvent = chrome.automation.AutomationEvent;
type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

export class FocusAutomationHandler extends BaseAutomationHandler {
  private previousActiveDescendant_?: AutomationNode;

  static instance: FocusAutomationHandler;

  private async initListener_(): Promise<void> {
    const desktop = await AsyncUtil.getDesktop();
    desktop.addEventListener(
        EventType.FOCUS, (node: AutomationEvent) => this.onFocus(node), false);
  }

  static async init(): Promise<void> {
    if (FocusAutomationHandler.instance) {
      throw new Error(
          'Trying to create two instances of singleton FocusAutomationHandler');
    }
    FocusAutomationHandler.instance = new FocusAutomationHandler(undefined);
    await FocusAutomationHandler.instance.initListener_();
  }

  onFocus(evt: AutomationEvent): void {
    this.removeAllListeners();

    // Events on roots and web views can be very noisy due to bubbling. Ignore
    // these.
    if (evt.target.root === evt.target ||
        evt.target.role === RoleType.WEB_VIEW) {
      return;
    }

    this.previousActiveDescendant_ = evt.target.activeDescendant;
    this.node_ = evt.target;
    this.addListener_(
        EventType.ACTIVE_DESCENDANT_CHANGED, this.onActiveDescendantChanged);
    this.addListener_(EventType.DETAILS_CHANGED, this.onDetailsChanged);
    this.addListener_(EventType.MENU_ITEM_SELECTED, this.onEventIfSelected);
    this.addListener_(
        EventType.SELECTED_VALUE_CHANGED, this.onSelectedValueChanged_);
  }

  /** Handles active descendant changes. */
  async onActiveDescendantChanged(evt: AutomationEvent): Promise<void> {
    if (!evt.target.activeDescendant) {
      if (ChromeVoxRange.current?.equals(CursorRange.fromNode(evt.target))) {
        new Output()
            .withLocation(ChromeVoxRange.current, undefined, evt.type)
            .go();
      }
      return;
    }

    let skipFocusCheck = false;
    const focus = await AsyncUtil.getFocus();
    if (focus !== null && AutomationPredicate.popUpButton(focus)) {
      skipFocusCheck = true;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!skipFocusCheck && !evt.target.state![StateType.FOCUSED]) {
      return;
    }

    // Various events might come before a key press (which forces flushed
    // speech) and this handler. Force output to be at least category flushed.
    Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);

    const prev = this.previousActiveDescendant_ ?
        CursorRange.fromNode(this.previousActiveDescendant_) :
        ChromeVoxRange.current;
    new Output()
        .withoutHints()
        .withRichSpeechAndBraille(
            CursorRange.fromNode(evt.target.activeDescendant),
            prev ?? undefined, OutputCustomEvent.NAVIGATE)
        .go();
    this.previousActiveDescendant_ = evt.target.activeDescendant;
  }

  /** Informs users that details are now available. */
  onDetailsChanged(_evt: ChromeVoxEvent): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const range = ChromeVoxRange.current!;
    let node: AutomationNode | undefined = range.start?.node;
    while (node && (!node.details || !node.details.length)) {
      node = node.parent;
    }
    if (!node) {
      return;
    }

    // Note that we only output speech. Braille output shows the entire line, so
    // details output should not be based on an announcement like this. Don't
    // allow interruption of this announcement which can occur in a slew of
    // events (e.g. typing).
    new Output()
        .withInitialSpeechProperties(
            new TtsSpeechProperties({doNotInterrupt: true}))
        .formatForSpeech('@hint_details')
        .go();
  }

  onEventIfSelected(evt: ChromeVoxEvent): void {
    if (evt.target.selected) {
      this.onEventDefault(evt);
    }
  }

  private onSelectedValueChanged_(evt: ChromeVoxEvent): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!AutomationPredicate.popUpButton(evt.target) ||
        evt.target.state![StateType.EDITABLE]) {
      return;
    }

    // Focus might be on a container above the popup button.
    if (this.node_ !== evt.target) {
      return;
    }

    // Return early if the menu is expanded, to avoid double speech, as active
    // descendant events will also be received and announced.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (evt.target.state![StateType.EXPANDED]) {
      return;
    }

    if (evt.target.value) {
      const output = new Output();
      output.format('$value @describe_index($posInSet, $setSize)', evt.target);
      output.go();
      return;
    }
  }
}
