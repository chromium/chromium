// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constant to indicate selection index is not set.
const NO_SELECT_INDEX = -1;

/**
 * Class to handle navigating text. Currently, only
 * navigation and selection in editable text fields is supported.
 */
class TextNavigationManager {
  constructor() {
    /** @private {number} */
    this.selectionStartIndex_ = NO_SELECT_INDEX;

    /** @private {number} */
    this.selectionEndIndex_ = NO_SELECT_INDEX;

    /** @private {chrome.automation.AutomationNode} */
    this.selectionStartObject_;

    /** @private {chrome.automation.AutomationNode} */
    this.selectionEndObject_;

    /** @private {boolean} */
    this.currentlySelecting_ = false;

    /** @private {function(chrome.automation.AutomationEvent): undefined} */
    this.selectionListener_ = this.onNavChange_.bind(this);
  }

  /**
   * Jumps to the beginning of the text field (does nothing
   * if already at the beginning).
   */
  jumpToBeginning() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(false /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.HOME, {ctrl: true});
  }

  /**
   * Jumps to the end of the text field (does nothing if
   * already at the end).
   */
  jumpToEnd() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(false /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.END, {ctrl: true});
  }

  /**
   * Moves the text caret one character back (does nothing
   * if there are no more characters preceding the current
   * location of the caret).
   */
  moveBackwardOneChar() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(true /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.LEFT_ARROW);
  }

  /**
   * Moves the text caret one character forward (does nothing
   * if there are no more characters following the current
   * location of the caret).
   */
  moveForwardOneChar() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(true /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.RIGHT_ARROW);
  }

  /**
   * Moves the text caret one word backwards (does nothing
   * if already at the beginning of the field). If the
   * text caret is in the middle of a word, moves the caret
   * to the beginning of that word.
   */
  moveBackwardOneWord() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(false /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.LEFT_ARROW, {ctrl: true});
  }

  /**
   * Moves the text caret one word forward (does nothing if
   * already at the end of the field). If the text caret is
   * in the middle of a word, moves the caret to the end of
   * that word.
   */
  moveForwardOneWord() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(false /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.RIGHT_ARROW, {ctrl: true});
  }

  /**
   * Moves the text caret one line up (does nothing
   * if there are no lines above the current location of
   * the caret).
   */
  moveUpOneLine() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(true /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.UP_ARROW);
  }

  /**
   * Moves the text caret one line down (does nothing
   * if there are no lines below the current location of
   * the caret).
   */
  moveDownOneLine() {
    if (this.currentlySelecting_) {
      this.setupDynamicSelection_(true /* resetCursor */);
    }
    EventHelper.simulateKeyPress(EventHelper.KeyCode.DOWN_ARROW);
  }

  /**
   * TODO(crbug.com/999400): Work on text selection dynamic highlight and
   * text selection implementation below
   */

  /**
   * Sets up the cursor position and selection listener for dynamic selection.
   * If the needToResetCursor boolean is true, the function will move the cursor
   * to the end point of the selection before adding the event listener. If not,
   * it will simply add the listener.
   * @param {boolean} needToResetCursor
   * @private
   */
  setupDynamicSelection_(needToResetCursor) {
    if (needToResetCursor) {
      if (this.currentlySelecting() &&
          this.selectionEndIndex_ != NO_SELECT_INDEX) {
        // Move the cursor to the end of the existing selection.
        chrome.automation.setDocumentSelection({
          anchorObject: this.selectionEndObject_,
          anchorOffset: this.selectionEndIndex_,
          focusObject: this.selectionEndObject_,
          focusOffset: this.selectionEndIndex_
        });
      }
    }
    this.manageNavigationListener_(true /** Add the listener */);
  }

  /**
   * Sets the selection using the selectionStart and selectionEnd
   * as the offset input for setDocumentSelection and the parameter
   * textNode as the object input for setDocumentSelection.
   * @private
   */
  saveSelection_() {
    if (this.selectionStartIndex_ == NO_SELECT_INDEX ||
        this.selectionEndIndex_ == NO_SELECT_INDEX) {
      console.log(
          'Selection bounds are not set properly:', this.selectionStartIndex_,
          this.selectionEndIndex_);
    } else {
      chrome.automation.setDocumentSelection({
        anchorObject: this.selectionStartObject_,
        anchorOffset: this.selectionStartIndex_,
        focusObject: this.selectionEndObject_,
        focusOffset: this.selectionEndIndex_
      });
    }
  }

  /**
   * Returns the selection end index.
   * @return {number}
   */
  getSelEndIndex() {
    return this.selectionEndIndex_;
  }

  /**
   * Reset the selectionStartIndex to NO_SELECT_INDEX.
   */
  resetSelStartIndex() {
    this.selectionStartIndex_ = NO_SELECT_INDEX;
  }

  /**
   * Returns the selection start index.
   * @return {number}
   */
  getSelStartIndex() {
    return this.selectionStartIndex_;
  }

  /**
   * Sets the selection start index.
   * @param {number} startIndex
   * @param {!chrome.automation.AutomationNode} textNode
   */
  setSelStartIndexAndNode(startIndex, textNode) {
    this.selectionStartIndex_ = startIndex;
    this.selectionStartObject_ = textNode;
  }

  /**
   * Returns if the selection start index is set in the current node.
   * @return {boolean}
   */
  currentlySelecting() {
    return (
        this.selectionStartIndex_ !== NO_SELECT_INDEX &&
        this.currentlySelecting_);
  }

  /**
   * Returns either the selection start index or the selection end index of the
   * node based on the getStart param.
   * @param {!chrome.automation.AutomationNode} node
   * @param {boolean} getStart
   * @return {number} selection start if getStart is true otherwise selection
   * end
   * @private
   */
  getSelectionIndexFromNode_(node, getStart) {
    let indexFromNode = NO_SELECT_INDEX;
    if (getStart) {
      indexFromNode = node.textSelStart;
    } else {
      indexFromNode = node.textSelEnd;
    }
    if (indexFromNode === undefined) {
      return NO_SELECT_INDEX;
    }
    return indexFromNode;
  }

  /**
   * Sets the selectionStart variable based on the selection of the current
   * node. Also sets the currently selecting boolean to true.
   */
  saveSelectStart() {
    chrome.automation.getFocus((focusedNode) => {
      this.selectionStartObject_ = focusedNode;
      this.selectionStartIndex_ = this.getSelectionIndexFromNode_(
          this.selectionStartObject_,
          true /* We are getting the start index.*/);
      this.currentlySelecting_ = true;
    });
  }

  /**
   * Function to handle changes in the cursor position during selection.
   * This function will remove the selection listener and set the end of the
   * selection based on the new position.
   * @private
   */
  onNavChange_() {
    this.manageNavigationListener_(false);
    if (this.currentlySelecting) {
      this.saveSelectEnd();
    }
  }

  /**
   * Adds or removes the selection listener based on a boolean parameter.
   * @param {boolean} addListener
   * @private
   */
  manageNavigationListener_(addListener) {
    if (addListener) {
      this.selectionStartObject_.addEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          this.selectionListener_, false /** Don't use capture.*/);
    } else {
      this.selectionStartObject_.removeEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          this.selectionListener_, false /** Don't use capture.*/);
    }
  }

  /**
   * Reset the currentlySelecting variable to false, reset the selection
   * indices, and remove the listener on navigation.
   */
  resetCurrentlySelecting() {
    this.currentlySelecting_ = false;
    this.manageNavigationListener_(false /** Removing listener */);
    this.selectionStartIndex_ = NO_SELECT_INDEX;
    this.selectionEndIndex_ = NO_SELECT_INDEX;
  }

  /**
   * Sets the selectionEnd variable based on the selection of the current node.
   */
  saveSelectEnd() {
    chrome.automation.getFocus((focusedNode) => {
      this.selectionEndObject_ = focusedNode;
      this.selectionEndIndex_ = this.getSelectionIndexFromNode_(
          this.selectionEndObject_,
          false /*We are not getting the start index.*/);
      this.saveSelection_();
    });
  }
}
