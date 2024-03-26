// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '/common/event_generator.js';
import {EventHandler} from '/common/event_handler.js';
import {KeyCode} from '/common/key_code.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ActionManager} from './action_manager.js';
import {Navigator} from './navigator.js';
import {SwitchAccess} from './switch_access.js';
import {ErrorType} from './switch_access_constants.js';

type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;

/**
 * Class to handle navigating text. Currently, only
 * navigation and selection in editable text fields is supported.
 */
export class TextNavigationManager {
  private static instance_?: TextNavigationManager;

  private currentlySelecting_ = false;
  /** Keeps track of when there's a selection in the current node. */
  private selectionExists_ = false;
  /** Keeps track of when the clipboard is empty. */
  private clipboardHasData_ = false;

  private selectionStartIndex_ = TextNavigationManager.NO_SELECT_INDEX;
  private selectionStartObject_?: AutomationNode;
  private selectionEndIndex_ = TextNavigationManager.NO_SELECT_INDEX;
  private selectionEndObject_?: AutomationNode;
  private selectionListener_: EventHandler;

  private constructor() {
    this.selectionListener_ = new EventHandler(
        [], EventType.TEXT_SELECTION_CHANGED, () => this.onNavChange_());

    if (SwitchAccess.improvedTextInputEnabled()) {
      chrome.clipboard.onClipboardDataChanged.addListener(
          () => this.updateClipboardHasData_());
    }
  }

  static get instance(): TextNavigationManager {
    if (!TextNavigationManager.instance_) {
      TextNavigationManager.instance_ = new TextNavigationManager();
    }
    return TextNavigationManager.instance_;
  }

  // =============== Static Methods ==============

  /**
   * Returns if the selection start index is set in the current node.
   */
  static currentlySelecting(): boolean {
    const manager = TextNavigationManager.instance;
    return (
        manager.selectionStartIndex_ !==
            TextNavigationManager.NO_SELECT_INDEX &&
        manager.currentlySelecting_);
  }

  /**
   * Jumps to the beginning of the text field (does nothing
   * if already at the beginning).
   */
  static jumpToBeginning(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(false /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.HOME, {ctrl: true});
  }

  /**
   * Jumps to the end of the text field (does nothing if
   * already at the end).
   */
  static jumpToEnd(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(false /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.END, {ctrl: true});
  }

  /**
   * Moves the text caret one character back (does nothing
   * if there are no more characters preceding the current
   * location of the caret).
   */
  static moveBackwardOneChar(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(true /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.LEFT);
  }

  /**
   * Moves the text caret one word backwards (does nothing
   * if already at the beginning of the field). If the
   * text caret is in the middle of a word, moves the caret
   * to the beginning of that word.
   */
  static moveBackwardOneWord(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(false /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.LEFT, {ctrl: true});
  }

  /**
   * Moves the text caret one line down (does nothing
   * if there are no lines below the current location of
   * the caret).
   */
  static moveDownOneLine(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(true /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.DOWN);
  }

  /**
   * Moves the text caret one character forward (does nothing
   * if there are no more characters following the current
   * location of the caret).
   */
  static moveForwardOneChar(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(true /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.RIGHT);
  }

  /**
   * Moves the text caret one word forward (does nothing if
   * already at the end of the field). If the text caret is
   * in the middle of a word, moves the caret to the end of
   * that word.
   */
  static moveForwardOneWord(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(false /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.RIGHT, {ctrl: true});
  }

  /**
   * Moves the text caret one line up (does nothing
   * if there are no lines above the current location of
   * the caret).
   */
  static moveUpOneLine(): void {
    const manager = TextNavigationManager.instance;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(true /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.UP);
  }

  /**
   * Reset the currentlySelecting variable to false, reset the selection
   * indices, and remove the listener on navigation.
   */
  static resetCurrentlySelecting(): void {
    const manager = TextNavigationManager.instance;
    manager.currentlySelecting_ = false;
    manager.manageNavigationListener_(false /** Removing listener */);
    manager.selectionStartIndex_ = TextNavigationManager.NO_SELECT_INDEX;
    manager.selectionEndIndex_ = TextNavigationManager.NO_SELECT_INDEX;
    if (manager.currentlySelecting_) {
      manager.setupDynamicSelection_(true /* resetCursor */);
    }
    EventGenerator.sendKeyPress(KeyCode.DOWN);
  }

  static get clipboardHasData(): boolean {
    return TextNavigationManager.instance.clipboardHasData_;
  }

  static get selectionExists(): boolean {
    return TextNavigationManager.instance.selectionExists_;
  }

  static set selectionExists(newVal: boolean) {
    TextNavigationManager.instance.selectionExists_ = newVal;
  }

  getSelEndIndex(): number {
    return this.selectionEndIndex_;
  }

  resetSelStartIndex(): void {
    this.selectionStartIndex_ = TextNavigationManager.NO_SELECT_INDEX;
  }

  getSelStartIndex(): number {
    return this.selectionStartIndex_;
  }

  setSelStartIndexAndNode(startIndex: number, textNode: AutomationNode): void {
    this.selectionStartIndex_ = startIndex;
    this.selectionStartObject_ = textNode;
  }

  /**
   * Sets the selectionStart variable based on the selection of the current
   * node. Also sets the currently selecting boolean to true.
   */
  static saveSelectStart(): void {
    const manager = TextNavigationManager.instance;
    chrome.automation.getFocus((focusedNode: AutomationNode | undefined) => {
      manager.selectionStartObject_ = focusedNode;
      manager.selectionStartIndex_ = manager.getSelectionIndexFromNode_(
          // TODO(b/314203187): Not null asserted, check that this is correct.
          manager.selectionStartObject_!,
          true /* We are getting the start index.*/);
      manager.currentlySelecting_ = true;
    });
  }

  // =============== Instance Methods ==============

  /**
   * Returns either the selection start index or the selection end index of the
   * node based on the getStart param.
   * @return selection start if getStart is true otherwise selection
   * end
   */
  private getSelectionIndexFromNode_(
      node: AutomationNode, getStart: boolean): number {
    let indexFromNode = TextNavigationManager.NO_SELECT_INDEX;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (getStart) {
      indexFromNode = node.textSelStart!;
    } else {
      indexFromNode = node.textSelEnd!;
    }
    if (indexFromNode === undefined) {
      return TextNavigationManager.NO_SELECT_INDEX;
    }
    return indexFromNode;
  }

  /** Adds or removes the selection listener. */
  private manageNavigationListener_(addListener: boolean): void {
    if (!this.selectionStartObject_) {
      return;
    }

    if (addListener) {
      this.selectionListener_.setNodes(this.selectionStartObject_);
      this.selectionListener_.start();
    } else {
      this.selectionListener_.stop();
    }
  }

  /**
   * Function to handle changes in the cursor position during selection.
   * This function will remove the selection listener and set the end of the
   * selection based on the new position.
   */
  private onNavChange_(): void {
    this.manageNavigationListener_(false);
    if (this.currentlySelecting_) {
      TextNavigationManager.saveSelectEnd();
    }
  }

  /**
   * Sets the selectionEnd variable based on the selection of the current node.
   */
  static saveSelectEnd(): void {
    const manager = TextNavigationManager.instance;
    chrome.automation.getFocus(focusedNode => {
      manager.selectionEndObject_ = focusedNode;
      manager.selectionEndIndex_ = manager.getSelectionIndexFromNode_(
          manager.selectionEndObject_,
          false /*We are not getting the start index.*/);
      manager.saveSelection_();
    });
  }

  /** Sets the selection after verifying that the bounds are set. */
  private saveSelection_(): void {
    if (this.selectionStartIndex_ === TextNavigationManager.NO_SELECT_INDEX ||
        this.selectionEndIndex_ === TextNavigationManager.NO_SELECT_INDEX) {
      console.error(SwitchAccess.error(
          ErrorType.INVALID_SELECTION_BOUNDS,
          'Selection bounds are not set properly: ' +
              this.selectionStartIndex_ + ' ' + this.selectionEndIndex_));
    } else {
      this.setSelection_();
    }
  }

  /**
   * Sets up the cursor position and selection listener for dynamic selection.
   * If the needToResetCursor boolean is true, the function will move the cursor
   * to the end point of the selection before adding the event listener. If not,
   * it will simply add the listener.
   */
  private setupDynamicSelection_(needToResetCursor: boolean): void {
    /**
     * TODO(crbug.com/999400): Work on text selection dynamic highlight and
     * text selection implementation.
     */
    if (needToResetCursor) {
      if (TextNavigationManager.currentlySelecting() &&
          this.selectionEndIndex_ !== TextNavigationManager.NO_SELECT_INDEX) {
        // Move the cursor to the end of the existing selection.
        this.setSelection_();
      }
    }
    this.manageNavigationListener_(true /** Add the listener */);
  }

  /**
   * Sets the selection. If start and end object are equal, uses
   * AutomationNode.setSelection. Otherwise calls
   * chrome.automation.setDocumentSelection.
   */
  private setSelection_(): void {
    if (this.selectionStartObject_ === this.selectionEndObject_) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      this.selectionStartObject_!.setSelection(
          this.selectionStartIndex_, this.selectionEndIndex_);
    } else {
      chrome.automation.setDocumentSelection({
        anchorObject: this.selectionStartObject_!,
        anchorOffset: this.selectionStartIndex_,
        focusObject: this.selectionEndObject_!,
        focusOffset: this.selectionEndIndex_,
      });
    }
  }

  /*
   * TODO(rosalindag): Add functionality to catch when clipboardHasData_ needs
   * to be set to false.
   * Set the clipboardHasData variable to true and reload the menu.
   */
  private updateClipboardHasData_(): void {
    this.clipboardHasData_ = true;
    const node = Navigator.byItem.currentNode;
    if (node.hasAction(MenuAction.PASTE)) {
      ActionManager.refreshMenuForNode(node);
    }
  }
}

export namespace TextNavigationManager {
  export const NO_SELECT_INDEX = -1;
}

TestImportManager.exportForTesting(TextNavigationManager);
