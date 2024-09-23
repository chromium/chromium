// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {LocaleInfo} from './locale_info.js';

import HintType = chrome.accessibilityPrivate.DictationBubbleHintType;
import IconType = chrome.accessibilityPrivate.DictationBubbleIconType;

/** States the Dictation bubble UI can be in. */
/* eslint-disable @typescript-eslint/naming-convention */
export enum UIState {
  STANDBY = 'standby',
  RECOGNIZING_TEXT = 'recognizing_text',
  MACRO_SUCCESS = 'macro_success',
  MACRO_FAIL = 'macro_fail',
  HIDDEN = 'hidden',
}

/** Contexts in which hints can be shown. */
export enum HintContext {
  STANDBY = 'standby',
  TEXT_COMMITTED = 'text_committed',
  TEXT_SELECTED = 'text_selected',
  MACRO_SUCCESS = 'macro_success',
}

interface StateProperties {
  text?: string;
  context?: HintContext;
}

/**
 * Handles interaction with the Dictation UI. All changes to the UI should go
 * this class.
 */
/* eslint-disable @typescript-eslint/naming-convention */
export class UIController {
  private showHintsTimeoutId_: number|null = null;
  private showHintsTimeoutMs_: number =
      UIController.HintsTimeouts.STANDARD_HINT_TIMEOUT_MS_;

  /**
   * Sets the new state of the Dictation bubble UI. If a HintContext is
   * specified, additional hints will appear in the UI after a short timeout.
   */
  setState(state: UIState, properties?: StateProperties): void {
    const props = properties || {};
    const {text, context} = props;
    // Whenever the UI state changes, we should clear the hint timeout.
    this.clearHintsTimeout_();
    switch (state) {
      case UIState.STANDBY:
        chrome.accessibilityPrivate.updateDictationBubble(
            {visible: true, icon: IconType.STANDBY});
        break;
      case UIState.RECOGNIZING_TEXT:
        chrome.accessibilityPrivate.updateDictationBubble(
            {visible: true, icon: IconType.HIDDEN, text});
        break;
      case UIState.MACRO_SUCCESS:
        chrome.accessibilityPrivate.updateDictationBubble(
            {visible: true, icon: IconType.MACRO_SUCCESS, text});
        break;
      case UIState.MACRO_FAIL:
        chrome.accessibilityPrivate.updateDictationBubble(
            {visible: true, icon: IconType.MACRO_FAIL, text});
        break;
      case UIState.HIDDEN:
        chrome.accessibilityPrivate.updateDictationBubble(
            {visible: false, icon: IconType.HIDDEN});
        break;
    }

    if (!context || !LocaleInfo.areCommandsSupported()) {
      // Do not show hints if commands are not supported.
      return;
    }

    // If a HintContext was provided, set a timeout to show hints.
    const hints = UIController.getHintsForContext_(context);
    this.showHintsTimeoutId_ =
        setTimeout(() => this.showHints_(hints), this.showHintsTimeoutMs_);
  }

  private clearHintsTimeout_(): void {
    if (this.showHintsTimeoutId_ !== null) {
      clearTimeout(this.showHintsTimeoutId_);
      this.showHintsTimeoutId_ = null;
    }
  }

  /** Shows hints in the UI bubble. */
  private showHints_(hints: HintType[]): void {
    chrome.accessibilityPrivate.updateDictationBubble(
        {visible: true, icon: IconType.STANDBY, hints});
  }

  /**
   * In some circumstances we shouldn't show the hints too quickly because
   * it is distracting to the user.
   * @param longerDuration Whether to wait for a longer time before showing
   *     hints.
   */
  setHintsTimeoutDuration(longerDuration: boolean): void {
    this.showHintsTimeoutMs_ = longerDuration ?
        UIController.HintsTimeouts.LONGER_HINT_TIMEOUT_MS_ :
        UIController.HintsTimeouts.STANDARD_HINT_TIMEOUT_MS_;
  }

  private static getHintsForContext_(context: HintContext): HintType[] {
    return UIController.CONTEXT_TO_HINTS_MAP_[context];
  }
}

export namespace UIController {
  /** The amount of time to wait before showing hints. */
  export const HintsTimeouts = {
    STANDARD_HINT_TIMEOUT_MS_: 2 * 1000,
    LONGER_HINT_TIMEOUT_MS_: 6 * 1000,
  };

  /** Maps HintContexts to hints that should be shown for that context. */
  export const CONTEXT_TO_HINTS_MAP_ = {
    [HintContext.STANDBY]: [HintType.TRY_SAYING, HintType.TYPE, HintType.HELP],
    [HintContext.TEXT_COMMITTED]:
        [
          HintType.TRY_SAYING,
          HintType.UNDO,
          HintType.DELETE,
          HintType.SELECT_ALL,
          HintType.HELP,
        ],
    [HintContext.TEXT_SELECTED]:
        [
          HintType.TRY_SAYING,
          HintType.UNSELECT,
          HintType.COPY,
          HintType.DELETE,
          HintType.HELP,
        ],
    [HintContext.MACRO_SUCCESS]:
        [HintType.TRY_SAYING, HintType.UNDO, HintType.HELP],
  };
}

TestImportManager.exportForTesting(UIController);
