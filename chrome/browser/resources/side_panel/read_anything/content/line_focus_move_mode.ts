// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';

import type {Segment} from '../read_aloud/read_aloud_types.js';
import {getRectIndexAtY, getRectsForSegments} from '../shared/dom_queries.js';
import {calculateTextBounds} from '../shared/rect_calculations.js';

import type {LineFocusModel} from './line_focus_model.js';
import type {LineFocusStyleMode} from './line_focus_style_mode.js';
import {LineFocusMovement} from './read_anything_types.js';

// Used to prevent microadjustments of the line focus window when adjusting to
// new line heights as it can be distracting for no functional difference.
// Determined by experimentation and should be tweaked as needed.
const SCROLL_THRESHOLD = 10;

// Interface for communicating notifications back to the main
// LineFocusController.
export interface MoveModeDelegate {
  // Notifies that the focus element has moved.
  notifyMove(): void;

  // Notifies that the view needs to scroll.
  notifyScroll(scrollDiff: number, instant?: boolean): void;

  // Notifies that the view should scroll to the top of the content.
  notifyScrollToTop(): void;

  // Notifies that the content panel needs a scroll buffer to allow for
  // centering focus.
  notifyScrollBuffer(needsBuffer: boolean): void;

  // Notifies that a line focus session has ended.
  onSessionEnd(): void;
}

// Base class for line focus movement strategies.
export abstract class LineFocusMoveMode {
  constructor(
      protected model_: LineFocusModel,
      protected styleMode_: LineFocusStyleMode,
      protected delegate_: MoveModeDelegate) {}

  // Returns the movement type of this movement strategy.
  abstract getMovement(): LineFocusMovement;

  // Called when this movement mode becomes the active strategy.
  abstract onActivated(container: HTMLElement, height: number): void;

  // Called when the mouse is moved vertically within the content panel.
  abstract onMouseMove(y: number): void;

  // Called when the mouse is moved vertically within the toolbar.
  abstract onMouseMoveInToolbar(y: number): void;

  // Called when the location or size of the text lines may have changed.
  abstract onTextLocationsChange(container: HTMLElement, height: number): void;

  // Called when a scroll event finishes in the content panel.
  abstract onScrollEnd(newScrollTop: number): void;

  // Snaps the focus to the next or previous text line.
  snapToNextLine(isForward: boolean): boolean {
    const lines = this.model_.getTextBounds();
    if (!lines.length) {
      return false;
    }

    // If this is the first time snapping after mouse movement, move to the
    // closest line to the current Y.
    const currentIndex = this.model_.getCurrentLineIndex();
    if (currentIndex === null) {
      this.initializeSnapIndex(isForward);
      const linesToLog = this.styleMode_.getStyle().lines;
      for (let i = 0; i < linesToLog; i++) {
        chrome.readingMode.incrementLineFocusKeyboardLines();
      }
    } else {
      this.updateSnapIndex_(currentIndex, isForward);
    }

    return true;
  }

  // Updates focus when speech reaches a new word boundary.
  onWordBoundary(segments: Segment[]): void {
    const rects = getRectsForSegments(segments);
    if (rects.length === 0) {
      return;
    }
    const rect = rects[0]!;
    if (Math.abs(this.model_.getFocalPoint() -
                 this.styleMode_.getFocalPointForRect(rect)) > 0.1) {
      chrome.readingMode.incrementLineFocusSpeechLines();
    }
    this.moveToRect(rect);
    this.recenterCurrentTextLineIfOffScreen(/*instant=*/ false);
  }

  // Returns whether this move mode needs padding to reach all text.
  protected abstract needsScrollBuffer(): boolean;

  // Updates the focal point Y position or scrolls the view to the given
  // rect, depending on the movement strategy.
  protected abstract moveToRect(rect: DOMRect): void;

  protected setFocalPoint(focalPointY: number, quietly: boolean = false): void {
    this.model_.setFocalPoint(focalPointY);
    this.styleMode_.updateFocusBounds();
    if (!quietly) {
      this.delegate_.notifyMove();
    }
  }

  protected getSafeIndex(isForward: boolean): number {
    const lines = this.model_.getTextBounds();
    const rawIndex =
        getRectIndexAtY(this.model_.getFocalPoint(), lines, isForward);
    return this.styleMode_.clampLineIndex(rawIndex);
  }

  protected recenterCurrentTextLineIfOffScreen(instant: boolean): boolean {
    const bounds = this.model_.getTextBounds();
    if (bounds.length === 0) {
      return false;
    }

    const currentIndex = this.model_.getCurrentLineIndex() ??
        this.getSafeIndex(/*isForward=*/ true);
    const scrollDiff = this.styleMode_.getOffScreenDiff(currentIndex);
    if (Math.abs(scrollDiff) > SCROLL_THRESHOLD) {
      this.scroll(scrollDiff, instant);
      return true;
    }

    return false;
  }

  protected scroll(scrollDiff: number, instant?: boolean): void {
    if (Math.abs(scrollDiff) < SCROLL_THRESHOLD) {
      return;
    }
    this.model_.setInitiatedScroll(true);
    this.delegate_.notifyScroll(scrollDiff, instant);
  }

  protected resetScrollState(newScrollTop: number) {
    const distance =
        Math.round(Math.abs(newScrollTop - this.model_.getLastScrollTop()));
    chrome.readingMode.addLineFocusScrollDistance(distance);
    this.model_.setLastScrollTop(newScrollTop);

    // If the scroll was not initiated by line focus, then reset which line is
    // currently focused.
    if (!this.model_.getInitiatedScroll()) {
      this.model_.setCurrentLineIndex(null);
    }

    this.model_.setInitiatedScroll(false);
  }

  protected initializeSnapIndex(isForward: boolean) {
    const lines = this.model_.getTextBounds();
    const safeIndex = this.getSafeIndex(isForward);
    this.model_.setCurrentLineIndex(safeIndex);
    assert(safeIndex < lines.length);
    this.moveToRect(lines[safeIndex]!);
  }

  protected updatePositions(container: HTMLElement, height: number): void {
    const {minY, maxY, bounds} = calculateTextBounds(container, height);
    this.model_.setMinY(minY);
    this.model_.setMaxY(maxY);
    this.model_.setTextBounds(bounds);
  }

  // Common setup logic for when a movement mode that enables line focus is
  // activated.
  protected setupEnabledMode(container: HTMLElement, height: number): void {
    this.model_.setLastEnabledLineFocusStyle(this.styleMode_.getStyle());
    if (!this.model_.isSessionActive()) {
      chrome.readingMode.startLineFocusSession();
      this.model_.setSessionActive(true);
    }
    this.updatePositions(container, height);
    this.updateScrollBuffer();
  }

  protected updateScrollBuffer(): void {
    this.delegate_.notifyScrollBuffer(this.needsScrollBuffer());
  }

  protected getCenterY(): number {
    return (this.model_.getMaxY()) / 2;
  }

  protected notifyScrollToTop(): void {
    this.delegate_.notifyScrollToTop();
  }

  private updateSnapIndex_(currentIndex: number, isForward: boolean) {
    const lines = this.model_.getTextBounds();
    assert(lines.length > 0);
    const direction = isForward ? 1 : -1;
    const nextIndex = currentIndex + direction;
    if (nextIndex < 0 ||
        this.styleMode_.getBottomIndex(nextIndex) >= lines.length) {
      return;
    }

    const clampedIndex = this.styleMode_.clampLineIndex(nextIndex);
    this.model_.setCurrentLineIndex(clampedIndex);

    if (this.recenterCurrentTextLineIfOffScreen(/*instant=*/ false)) {
      chrome.readingMode.incrementLineFocusKeyboardLines();
    } else if (this.model_.getCurrentLineIndex() !== currentIndex) {
      chrome.readingMode.incrementLineFocusKeyboardLines();
      this.moveToRect(lines[clampedIndex]!);
    }

    // If the user has navigated back to the top of the panel, but there's
    // still a little bit left to scroll, scroll to the top.
    if (this.model_.getCurrentLineIndex() === currentIndex) {
      this.notifyScrollToTop();
    }
  }
}

// Movement strategy where the focus element stays centered in the view,
// scrolling the view when needed.
export class LineFocusStaticMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.STATIC;
  }

  onActivated(container: HTMLElement, height: number): void {
    this.setupEnabledMode(container, height);
    // In static mode, don't adapt the window size to line height to prevent
    // jarring movement / jitter.
    this.model_.setAdaptMultiLineWindow(false);
    this.setFocalPoint(this.getCenterY());
  }

  // Static mode ignores mouse movements.
  onMouseMove(_y: number): void {}
  onMouseMoveInToolbar(_y: number): void {}

  onScrollEnd(newScrollTop: number): void {
    const initiatedScroll = this.model_.getInitiatedScroll();
    this.resetScrollState(newScrollTop);
    // For a user-initiated scroll, notify that the focus is in a different
    // position in the content even though the coordinates are the same. For a
    // line-focus-initiated scroll, only notify if the focus changed.
    if (!initiatedScroll || this.styleMode_.updateAfterScroll()) {
      this.delegate_.notifyMove();
    }
  }

  onTextLocationsChange(container: HTMLElement, height: number): void {
    const previousMaxY = this.model_.getMaxY();
    const previousMinY = this.model_.getMinY();
    this.updatePositions(container, height);
    this.updateScrollBuffer();
    if (previousMaxY !== this.model_.getMaxY() ||
        previousMinY !== this.model_.getMinY()) {
      this.setFocalPoint(this.getCenterY());
    }
  }

  protected moveToRect(rect: DOMRect): void {
    const focalPoint = this.styleMode_.getFocalPointForRect(rect);
    const scrollDiff = focalPoint - this.model_.getFocalPoint();
    this.scroll(scrollDiff);
  }

  protected needsScrollBuffer(): boolean {
    return true;
  }
}

// Movement strategy where the focus element follows the mouse cursor.
export class LineFocusCursorMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.CURSOR;
  }

  onActivated(container: HTMLElement, height: number): void {
    const wasEnabled = this.model_.isSessionActive();
    this.setupEnabledMode(container, height);
    this.model_.setAdaptMultiLineWindow(true);
    if (!wasEnabled && this.model_.getTextBounds().length > 0) {
      this.initializeSnapIndex(/*isForward=*/ true);
    } else {
      this.setFocalPoint(
          Math.max(this.model_.getMinY(), this.model_.getFocalPoint()));
    }
  }

  onMouseMove(y: number): void {
    this.model_.setCurrentLineIndex(null);
    const previousFocalPoint = this.model_.getFocalPoint();
    this.setFocalPoint(Math.max(this.model_.getMinY(), y));
    chrome.readingMode.addLineFocusMouseDistance(
        Math.round(Math.abs(this.model_.getFocalPoint() - previousFocalPoint)));
  }

  onMouseMoveInToolbar(y: number): void {
    // Store the new position, but do not notify listeners since the mouse is
    // in the toolbar, which means they are likely trying to change some
    // settings. onAllMenusClose will notify them of the final position when
    // all the settings menus are closed.
    this.setFocalPoint(Math.max(this.model_.getMinY(), y), /*quietly=*/ true);
  }

  onScrollEnd(newScrollTop: number): void {
    this.resetScrollState(newScrollTop);
  }

  onTextLocationsChange(container: HTMLElement, height: number): void {
    const currentIndex = this.model_.getCurrentLineIndex();

    this.updatePositions(container, height);
    this.updateScrollBuffer();
    this.recenterCurrentTextLineIfOffScreen(/*instant=*/ true);

    if (currentIndex !== null) {
      const newFocalPoint = this.styleMode_.getDesiredCenter(currentIndex);
      this.setFocalPoint(newFocalPoint);
    } else if (this.model_.getMinY() > this.model_.getFocalPoint()) {
      this.initializeSnapIndex(/*isForward=*/ true);
    }
  }

  protected moveToRect(rect: DOMRect): void {
    const focalPoint = this.styleMode_.getFocalPointForRect(rect);
    this.setFocalPoint(focalPoint);
  }

  protected needsScrollBuffer(): boolean {
    return false;
  }
}

// Movement strategy for when line focus is disabled.
export class LineFocusNoneMoveMode extends LineFocusMoveMode {
  constructor(
      model: LineFocusModel, styleMode: LineFocusStyleMode,
      delegate: MoveModeDelegate, private movement_: LineFocusMovement) {
    super(model, styleMode, delegate);
  }

  getMovement(): LineFocusMovement {
    return this.movement_;
  }

  onActivated(_container: HTMLElement, _height: number): void {
    if (this.model_.isSessionActive()) {
      this.delegate_.onSessionEnd();
    }
    this.model_.reset();
    this.updateScrollBuffer();
  }

  onMouseMove(_y: number): void {}
  onMouseMoveInToolbar(_y: number): void {}
  onScrollEnd(_newScrollTop: number): void {}
  onTextLocationsChange(_container: HTMLElement, _height: number): void {}
  override onWordBoundary(_segments: Segment[]): void {}
  override snapToNextLine(_isForward: boolean): boolean {
    return false;
  }
  protected moveToRect(_rect: DOMRect): void {}
  protected needsScrollBuffer(): boolean {
    return false;
  }
}
