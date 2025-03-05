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

import {ChromeVoxRange} from './chromevox_range.js';

type AutomationNode = chrome.automation.AutomationNode;

export class CaptionsHandler {
  static instance: CaptionsHandler;

  private previousFocus_: CursorRange|null = null;
  private waitingForCaptions_ = false;

  static init(): void {
    CaptionsHandler.instance = new CaptionsHandler();
  }

  static open(): void {
    if (CaptionsHandler.instance.isPrefEnabled()) {
      CaptionsHandler.instance.tryJumpToCaptionBubble_();
    } else {
      CaptionsHandler.instance.enableLiveCaption_();
    }
  }

  static close(): void {
    CaptionsHandler.instance.disableLiveCaption_();
    CaptionsHandler.instance.restoreFocus_();
  }

  isPrefEnabled(): boolean {
    return SettingsManager.getBoolean(
        'accessibility.captions.live_caption_enabled', /*isChromeVox=*/ false);
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

  private disableLiveCaption_(): void {
    // TODO(crbug.com/395945468): Disable live caption when appropriate.
  }

  /** This function assumes the preference for live captions is false. */
  private enableLiveCaption_(): void {
    this.waitingForCaptions_ = true;
    chrome.accessibilityPrivate.enableLiveCaption(true);
  }

  private jumpToCaptionBubble_(captionsLabel: AutomationNode): void {
    this.previousFocus_ = ChromeVoxRange.current;
    ChromeVoxRange.navigateTo(CursorRange.fromNode(captionsLabel));
  }

  private restoreFocus_(): void {
    if (!this.previousFocus_) {
      return;
    }

    // TODO(crbug.com/395945468): Restore focus to where it was before jumping
    // to captions.
  }

  private tryFindCaptions_(node: AutomationNode): AutomationNode|undefined {
    return node.find({attributes: {className: 'CaptionBubbleLabel'}});
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

TestImportManager.exportForTesting(CaptionsHandler);
