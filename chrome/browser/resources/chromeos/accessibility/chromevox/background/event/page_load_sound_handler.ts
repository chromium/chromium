// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles page loading sounds based on automation events.
 */
import {AsyncUtil} from '/common/async_util.js';
import {AutomationUtil} from '/common/automation_util.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ChromeVoxEvent} from '../../common/custom_automation_event.js';
import {EarconId} from '../../common/earcon_id.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from '../chromevox_range.js';

import {BaseAutomationHandler} from './base_automation_handler.js';

const EventType = chrome.automation.EventType;
const StateType = chrome.automation.StateType;

export class PageLoadSoundHandler extends BaseAutomationHandler
    implements ChromeVoxRangeObserver {
  private didRequestLoadSound_ = false;

  static instance: PageLoadSoundHandler;

  private constructor() {
    super(undefined);
  }

  private async initListeners_(): Promise<void> {
    this.node_ = await AsyncUtil.getDesktop();

    this.addListener_(EventType.LOAD_COMPLETE, this.onLoadComplete);
    this.addListener_(EventType.LOAD_START, this.onLoadStart);

    ChromeVoxRange.addObserver(this);
  }

  static async init(): Promise<void> {
    if (PageLoadSoundHandler.instance) {
      throw 'Error: Trying to create two instances of singleton ' +
          'PageLoadSoundHandler';
    }
    PageLoadSoundHandler.instance = new PageLoadSoundHandler();
    await PageLoadSoundHandler.instance.initListeners_();
  }

  /** Stops page load sound on load complete. */
  onLoadComplete(evt: ChromeVoxEvent): void {
    // We are only interested in load completes on valid top level roots.
    const top = AutomationUtil.getTopLevelRoot(evt.target);
    if (!top || top !== evt.target.root || !top.docUrl) {
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (this.didRequestLoadSound_ && top.parent &&
        top.parent.state![StateType.FOCUSED]) {
      ChromeVox.earcons.playEarcon(EarconId.PAGE_FINISH_LOADING);
      this.didRequestLoadSound_ = false;
    }
  }

  /** Starts page load sound on load start. */
  onLoadStart(evt: ChromeVoxEvent): void {
    // We are only interested in load starts on focused top level roots.
    const top = AutomationUtil.getTopLevelRoot(evt.target);
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (top && top === evt.target.root && top.docUrl && top.parent &&
        top.parent.state![StateType.FOCUSED]) {
      ChromeVox.earcons.playEarcon(EarconId.PAGE_START_LOADING);
      this.didRequestLoadSound_ = true;
    }
  }

  /** ChromeVoxRangeObserver implementation */
  onCurrentRangeChanged(range: CursorRange): void {
    if (!range || !range.start || !range.start.node) {
      return;
    }

    const top = AutomationUtil.getTopLevelRoot(range.start.node);
    // |top| might be undefined e.g. if range is not in a root web area.
    if (this.didRequestLoadSound_ && (!top || top.docLoadingProgress === 1)) {
      ChromeVox.earcons.playEarcon(EarconId.PAGE_FINISH_LOADING);
      this.didRequestLoadSound_ = false;
    }

    // Note that we intentionally don't re-start progress playback here even if
    // the docLoadingProgress < 1.
  }
}

TestImportManager.exportForTesting(PageLoadSoundHandler);
