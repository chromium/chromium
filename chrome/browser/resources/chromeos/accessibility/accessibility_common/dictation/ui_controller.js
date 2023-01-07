// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocaleInfo} from './locale_info.js';

const HintType = chrome.accessibilityPrivate.DictationBubbleHintType;
const IconType = chrome.accessibilityPrivate.DictationBubbleIconType;

/**
 * States the Dictation bubble UI can be in.
 * @enum {string}
 */
export const UIState = {
  STANDBY: 'standby',
  RECOGNIZING_TEXT: 'recognizing_text',
  MACRO_SUCCESS: 'macro_success',
  MACRO_FAIL: 'macro_fail',
  HIDDEN: 'hidden',
};

/**
 * Contexts in which hints can be shown.
 * @enum {string}
 */
export const HintContext = {
  STANDBY: 'standby',
  TEXT_COMMITTED: 'text_committed',
  TEXT_SELECTED: 'text_selected',
  MACRO_SUCCESS: 'macro_success',
};

/**
 * Handles interaction with the Dictation UI. All changes to the UI should go
 * this class.
 */
export class UIController {
  constructor() {
    /** @private {?number} */
    this.showHintsTimeoutId_ = null;

    /** @private {number} */
    this.showHintsTimeoutMs_ =
        UIController.HintsTimeouts.STANDARD_HINT_TIMEOUT_MS_;
  }

  /**
   * Sets the new state of the Dictation bubble UI. If a HintContext is
   * specified, additional hints will appear in the UI after a short timeout.
   * @param {!UIState} state
   * @param {{text: (string|undefined), context: (!HintContext|undefined)}=}
   *     opt_properties
   */
  setState(state, opt_properties) {
    const props = opt_properties || {};
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
    const hints = UIController.GetHintsForContext_(context);
    this.showHintsTimeoutId_ =
        setTimeout(() => this.showHints_(hints), this.showHintsTimeoutMs_);
  }

  /** @private */
  clearHintsTimeout_() {
    if (this.showHintsTimeoutId_ !== null) {
      clearTimeout(this.showHintsTimeoutId_);
      this.showHintsTimeoutId_ = null;
    }
  }

  /**
   * Shows hints in the UI bubble.
   * @param {!Array<string>} hints
   * @private
   */
  showHints_(hints) {
    chrome.accessibilityPrivate.updateDictationBubble(
        {visible: true, icon: IconType.STANDBY, hints});
  }

  /**
   * In some circumstances we shouldn't show the hints too quickly because
   * it is distracting to the user.
   * @param {boolean} longerDuration Whether to wait for a longer time before
   *     showing hints.
   */
  setHintsTimeoutDuration(longerDuration) {
    this.showHintsTimeoutMs_ = longerDuration ?
        UIController.HintsTimeouts.LONGER_HINT_TIMEOUT_MS_ :
        UIController.HintsTimeouts.STANDARD_HINT_TIMEOUT_MS_;
  }

  /**
   * @param {!HintContext} context
   * @return {!Array<!HintType>}
   * @private
   */
  static GetHintsForContext_(context) {
    return UIController.CONTEXT_TO_HINTS_MAP_[context];
  }
}

/**
 * The amount of time to wait before showing hints.
 * @private {!Object<string, number>}
 * @const
 */
UIController.HintsTimeouts = {
  STANDARD_HINT_TIMEOUT_MS_: 2 * 1000,
  LONGER_HINT_TIMEOUT_MS_: 6 * 1000,
};

/**
 * Maps HintContexts to hints that should be shown for that context.
 * @private {!Object<!HintContext, !Array<string>>}
 * @const
 */
UIController.CONTEXT_TO_HINTS_MAP_ = {
  [HintContext.STANDBY]: [HintType.TRY_SAYING, HintType.TYPE, HintType.HELP],
  [HintContext.TEXT_COMMITTED]: [
    HintType.TRY_SAYING,
    HintType.UNDO,
    HintType.DELETE,
    HintType.SELECT_ALL,
    HintType.HELP,
  ],
  [HintContext.TEXT_SELECTED]: [
    HintType.TRY_SAYING,
    HintType.UNSELECT,
    HintType.COPY,
    HintType.DELETE,
    HintType.HELP,
  ],
  [HintContext.MACRO_SUCCESS]:
      [HintType.TRY_SAYING, HintType.UNDO, HintType.HELP],
};
