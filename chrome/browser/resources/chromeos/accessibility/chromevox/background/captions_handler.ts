// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages opening and closing the live caption bubble, as well
 * as moving ChromeVox focus to the bubble when it appears and restoring focus
 * when it goes away.
 */
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ChromeVoxEvent} from '../common/custom_automation_event.js';
import {SettingsManager} from '../common/settings_manager.js';

import {ChromeVoxRange, ChromeVoxRangeObserver} from './chromevox_range.js';

type AutomationNode = chrome.automation.AutomationNode;

export class CaptionsHandler implements ChromeVoxRangeObserver {
  static instance: CaptionsHandler;

  private hasAttributeChanged_ = false;
  private inCaptions_ = false;
  private previousFocus_: CursorRange|null = null;
  private previousPrefState_?: boolean;
  private waitingForCaptions_ = false;

  static init(): void {
    CaptionsHandler.instance = new CaptionsHandler();
  }

  static open(): void {
    CaptionsHandler.instance.saveLiveCaptionValue_();
    if (CaptionsHandler.instance.isLiveCaptionEnabled_()) {
      CaptionsHandler.instance.tryJumpToCaptionBubble_();
    } else {
      CaptionsHandler.instance.enableLiveCaption_();
    }
  }

  static close(): void {
    CaptionsHandler.instance.resetLiveCaption_();
    CaptionsHandler.instance.onExitCaptions_();
  }

  static inCaptions(): boolean {
    return CaptionsHandler.instance.inCaptions_;
  }

  static get hasAttributeChanged(): boolean {
    return CaptionsHandler.instance.hasAttributeChanged_;
  }

  static handleAttributeChanged(): void {
    CaptionsHandler.instance.hasAttributeChanged_ = true;
  }

  maybeHandleAlert(event: ChromeVoxEvent): boolean {
    if (!this.waitingForCaptions_) {
      return false;
    }

    const captionsLabel = this.tryFindCaptions_(event.target);
    if (!captionsLabel) {
      return false;
    }

    this.waitingForCaptions_ = false;
    this.jumpToCaptionBubble_(captionsLabel);
    return true;
  }

  // ChromeVoxRangeObserver implementation.
  onCurrentRangeChanged(range: CursorRange|null): void {
    if (!range) {
      return;
    }

    if (range.start.node.className !== CAPTION_BUBBLE_LABEL) {
      this.onExitCaptions_();
    }
  }

  /** This function assumes the preference for live captions is false. */
  private enableLiveCaption_(): void {
    this.waitingForCaptions_ = true;
    chrome.accessibilityPrivate.enableLiveCaption(true);
  }

  private isLiveCaptionEnabled_(): boolean {
    return SettingsManager.getBoolean(
        'accessibility.captions.live_caption_enabled', /*isChromeVox=*/ false);
  }

  private jumpToCaptionBubble_(captionsLabel: AutomationNode): void {
    this.previousFocus_ = ChromeVoxRange.current;
    this.onEnterCaptions_();
    ChromeVoxRange.navigateTo(CursorRange.fromNode(captionsLabel));
  }

  private onEnterCaptions_(): void {
    this.inCaptions_ = true;
    ChromeVoxRange.addObserver(this);
  }

  private onExitCaptions_(): void {
    this.inCaptions_ = false;
    this.hasAttributeChanged_ = false;
    ChromeVoxRange.removeObserver(this);
    this.restoreFocus_();
  }

  private resetLiveCaption_(): void {
    // Don't disable live captions if they were enabled before we began.
    if (this.previousPrefState_) {
      return;
    }
    chrome.accessibilityPrivate.enableLiveCaption(false);
  }

  private restoreFocus_(): void {
    if (!this.previousFocus_) {
      return;
    }

    ChromeVoxRange.navigateTo(this.previousFocus_);
  }

  private saveLiveCaptionValue_(): void {
    this.previousPrefState_ = this.isLiveCaptionEnabled_();
  }

  private tryFindCaptions_(node: AutomationNode): AutomationNode|undefined {
    return node.find({attributes: {className: CAPTION_BUBBLE_LABEL}});
  }

  /**
   * Attempts to locate the caption bubble in the accessibility tree. If found,
   * moves focus there.
   */
  private async tryJumpToCaptionBubble_(): Promise<void> {
    const desktop: AutomationNode =
        await new Promise(resolve => chrome.automation.getDesktop(resolve));
    const captionsLabel = this.tryFindCaptions_(desktop);
    if (!captionsLabel) {
      return;
    }

    this.jumpToCaptionBubble_(captionsLabel);
  }
}

const CAPTION_BUBBLE_LABEL = 'CaptionBubbleLabel';

TestImportManager.exportForTesting(CaptionsHandler);
