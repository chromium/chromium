// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles page loading sounds based on automation events.
 */
import {AutomationUtil} from '../../common/automation_util.js';
import {constants} from '../../common/constants.js';
import {Earcon} from '../common/abstract_earcons.js';
import {ChromeVoxEvent} from '../common/custom_automation_event.js';

import {BaseAutomationHandler} from './base_automation_handler.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxState, ChromeVoxStateObserver} from './chromevox_state.js';

const ActionType = chrome.automation.ActionType;
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/** @implements {ChromeVoxStateObserver} */
export class PageLoadSoundHandler extends BaseAutomationHandler {
  /** @private */
  constructor() {
    super(null);

    /** @private {boolean} */
    this.didRequestLoadSound_ = false;

    chrome.automation.getDesktop(desktop => {
      this.node_ = desktop;

      this.addListener_(EventType.LOAD_COMPLETE, this.onLoadComplete);
      this.addListener_(EventType.LOAD_START, this.onLoadStart);

      ChromeVoxState.addObserver(this);
    });
  }

  static init() {
    if (PageLoadSoundHandler.instance) {
      throw 'Error: Trying to create two instances of singleton PageLoadSoundHandler';
    }
    PageLoadSoundHandler.instance = new PageLoadSoundHandler();
  }

  /**
   * Stops page load sound on load complete.
   * @param {!ChromeVoxEvent} evt
   */
  onLoadComplete(evt) {
    // We are only interested in load completes on valid top level roots.
    const top = AutomationUtil.getTopLevelRoot(evt.target);
    if (!top || top !== evt.target.root || !top.docUrl) {
      return;
    }

    if (this.didRequestLoadSound_ && top.parent && top.parent.state.focused) {
      ChromeVox.earcons.playEarcon(Earcon.PAGE_FINISH_LOADING);
      this.didRequestLoadSound_ = false;
    }
  }

  /**
   * Starts page load sound on load start.
   * @param {!ChromeVoxEvent} evt
   */
  onLoadStart(evt) {
    // We are only interested in load starts on focused top level roots.
    const top = AutomationUtil.getTopLevelRoot(evt.target);
    if (top && top === evt.target.root && top.docUrl && top.parent &&
        top.parent.state.focused) {
      ChromeVox.earcons.playEarcon(Earcon.PAGE_START_LOADING);
      this.didRequestLoadSound_ = true;
    }
  }

  /** @override */
  onCurrentRangeChanged(range) {
    if (!range || !range.start || !range.start.node) {
      return;
    }

    const top = AutomationUtil.getTopLevelRoot(range.start.node);
    // |top| might be undefined e.g. if range is not in a root web area.
    if (this.didRequestLoadSound_ && (!top || top.docLoadingProgress === 1)) {
      ChromeVox.earcons.playEarcon(Earcon.PAGE_FINISH_LOADING);
      this.didRequestLoadSound_ = false;
    }

    // Note that we intentionally don't re-start progress playback here even if
    // the docLoadingProgress < 1.
  }
}

/** @type {PageLoadSoundHandler} */
PageLoadSoundHandler.instance;
