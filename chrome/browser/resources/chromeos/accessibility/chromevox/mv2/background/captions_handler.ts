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
import {Output} from './output/output.js';

type AutomationEvent = chrome.automation.AutomationEvent;
type AutomationNode = chrome.automation.AutomationNode;
import EventType = chrome.automation.EventType;

type Listener = (evt: AutomationEvent) => void;

interface LastOutputData {
  // Index of the line in buffer.
  lineIndex: number;
  // Start and end char range in the line that is outputted. Needed to handle
  // partial lines.
  start: number;
  end: number;
}

export class CaptionsHandler implements ChromeVoxRangeObserver {
  static instance: CaptionsHandler;

  private inCaptions_ = false;
  private previousFocus_: CursorRange|null = null;
  private previousPrefState_?: boolean;
  private waitingForCaptions_ = false;

  private captionsLabel_: AutomationNode|null = null;
  private boundOnCaptionsLabelChanged_: Listener|null = null;

  // Max lines of captions to keep around.
  static readonly MAX_LINES = 100;

  // Captions line buffer.
  private captionLines_: string[] = [];

  // Tracks the last data sent to the Braille display.
  private lastOutput_: LastOutputData = {
    lineIndex: -1,
    start: -1,
    end: -1,
  };

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

  // Returns true if the alert was handled and the default handing should be
  // skipped, false otherwise.
  maybeHandleAlert(event: ChromeVoxEvent): boolean {
    if (!this.waitingForCaptions_) {
      // Filter out captions bubble dialog alert and announcement when this is
      // in the bubble. Otherwise, the alert overwrites the braille output.
      if (this.inCaptions_ && this.captionsLabel_ &&
          (this.captionsLabel_ === this.tryFindCaptions_(event.target) ||
           (event.target.parent &&
            this.captionsLabel_ ===
                this.tryFindCaptions_(event.target.parent!)))) {
        return true;
      }

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

    const currentNode = range.start.node;
    if (currentNode.className !== CAPTION_BUBBLE_LABEL) {
      this.onExitCaptions_();
    } else {
      this.startObserveCaptions_(currentNode);
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
    ChromeVoxRange.navigateTo(
        CursorRange.fromNode(captionsLabel),
        /*focus=*/ true, /*speechProps=*/ undefined,
        /*skipSettingSelection=*/ undefined,
        /*skipOutput=*/ true);
  }

  private onEnterCaptions_(): void {
    this.inCaptions_ = true;
    ChromeVoxRange.addObserver(this);
  }

  private onExitCaptions_(): void {
    this.inCaptions_ = false;
    ChromeVoxRange.removeObserver(this);
    this.stopObserveCaptions_();
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

  /** Starts to observe live caption label. */
  private startObserveCaptions_(captionsLabel: AutomationNode): void {
    this.captionsLabel_ = captionsLabel;

    if (!this.boundOnCaptionsLabelChanged_) {
      this.boundOnCaptionsLabelChanged_ = () => this.onCaptionsLabelChanged_();
    }

    this.captionsLabel_.addEventListener(
        EventType.TEXT_CHANGED, this.boundOnCaptionsLabelChanged_, true);
    this.updateCaptions_();
  }

  /** Stops observing live caption label. */
  private stopObserveCaptions_(): void {
    if (!this.captionsLabel_) {
      return;
    }

    this.captionLines_ = [];
    this.resetLastOutput_();

    this.captionsLabel_.removeEventListener(
        EventType.TEXT_CHANGED, this.boundOnCaptionsLabelChanged_!, true);
    this.captionsLabel_ = null;
  }

  /** Invoked when captions label is changed. */
  private onCaptionsLabelChanged_(): void {
    this.updateCaptions_();
  }

  /**
   * Extracts the caption lines from captions bubble and merges with the line
   * buffer.
   */
  private updateCaptions_(): void {
    if (!this.captionsLabel_) {
      return;
    }

    let currentLines: string[] = [];
    for (let i = 0, child; child = this.captionsLabel_.children[i]; ++i) {
      if (child.name) {
        currentLines.push(child.name);
      }
    }

    this.mergeLines_(currentLines);
  }

  /** Merge the current lines in caption bubble with the line buffer. */
  private mergeLines_(currentLines: string[]): void {
    // Finds an insertion point to update the line buffer.
    const fullMatchFirstLine: number =
        this.captionLines_.lastIndexOf(currentLines[0]);
    if (fullMatchFirstLine !== -1) {
      // If first line of `currentLines` is found, copy `currentLines` at the
      // index.
      this.captionLines_.splice(
          fullMatchFirstLine, this.captionLines_.length - fullMatchFirstLine,
          ...currentLines);
    } else if (currentLines.length === 1 || this.captionLines_.length === 1) {
      // If buffer is initiating, just copying over. Reset tracking if
      // `currentLines` is not relevant.
      if (this.lastOutput_.lineIndex !== -1) {
        let lastOutputText =
            this.captionLines_[this.lastOutput_.lineIndex].substring(
                this.lastOutput_.start, this.lastOutput_.end);
        if (currentLines[0].indexOf(lastOutputText) === -1) {
          this.resetLastOutput_();
        }
      }

      this.captionLines_ = [...currentLines];
    } else {
      // Otherwise, restart tracking.
      this.captionLines_ = [...currentLines];
      this.resetLastOutput_();
    }

    // Trim when the line buffer is more than MAX_LINES.
    while (this.captionLines_.length > CaptionsHandler.MAX_LINES) {
      this.captionLines_.shift();
      this.lastOutput_.lineIndex--;
    }
    if (this.lastOutput_.lineIndex < 0) {
      this.resetLastOutput_();
    }

    if (this.lastOutput_.lineIndex === -1 && this.captionLines_.length > 0) {
      this.output_(0);
    }
  }

  /** Reset output tracking */
  private resetLastOutput_(): void {
    this.lastOutput_ = {
      lineIndex: -1,
      start: -1,
      end: -1,
    };
  }

  /** Outputs the given line. */
  private output_(index: number, start?: number, end?: number): void {
    let text = this.captionLines_[index];

    this.lastOutput_.lineIndex = index;
    this.lastOutput_.start = start ?? 0;
    this.lastOutput_.end = end ?? text.length;

    new Output()
        .withString(
            text.substring(this.lastOutput_.start, this.lastOutput_.end))
        .go();
  }

  /** Output the previous line if there is one. */
  previous(): void {
    if (this.lastOutput_.lineIndex == -1) {
      return;
    }

    // If the last output is in middle of the line, restart from the beginning.
    if (this.lastOutput_.start != 0) {
      this.output_(this.lastOutput_.lineIndex, 0);
      return;
    }

    // No more previous lines.
    if (this.lastOutput_.lineIndex == 0) {
      return;
    }

    this.output_(this.lastOutput_.lineIndex - 1);
  }

  /** Output the next line if there is one. */
  next(): void {
    if (this.lastOutput_.lineIndex == -1) {
      return;
    }

    // If the last output is not at the end, start from where it stops.
    if (this.captionLines_[this.lastOutput_.lineIndex].length >
        this.lastOutput_.end) {
      this.output_(this.lastOutput_.lineIndex, this.lastOutput_.end);
      return;
    }

    // No more next lines.
    if (this.lastOutput_.lineIndex == this.captionLines_.length - 1) {
      return;
    }

    this.output_(this.lastOutput_.lineIndex + 1);
  }
}

const CAPTION_BUBBLE_LABEL = 'CaptionBubbleLabel';

TestImportManager.exportForTesting(CaptionsHandler);
