// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';

import {getLineFocusValues, LineFocusMovement, LineFocusStyle, LineFocusType} from '../content/read_anything_types.js';
import type {Segment} from '../read_aloud/read_aloud_types.js';
import {SpeechController} from '../read_aloud/speech_controller.js';
import {getTextNodeOffsets} from '../shared/dom_queries.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {LineFocusModel} from './line_focus_model.js';

export interface LineFocusListener {
  onLineFocusMove(): void;
  onNeedScrollForLineFocus(scrollDiff: number): void;
  onNeedScrollToTop(): void;
  onLineFocusToggled(): void;
}

// Used to prevent microadjustments of the line focus window when adjusting to
// new line heights as it can be distracting for no functional difference.
// Determined by experimentation and should be tweaked as needed.
const WINDOW_DIFF_THRESHOLD = 5;
const SCROLL_THRESHOLD = 10;

// Handles the business logic for managing the line focus feature.
export class LineFocusController {
  private readonly listeners_: LineFocusListener[] = [];
  private model_: LineFocusModel = new LineFocusModel();
  private speechController_ = SpeechController.getInstance();
  private logger_ = ReadAnythingLogger.getInstance();

  getTop(): number {
    return this.model_.getTop();
  }

  getHeight(): number|null {
    return (this.getCurrentLineFocusType() === LineFocusType.WINDOW) ?
        this.model_.getWindowHeight() :
        null;
  }

  getCurrentLineFocusType(): LineFocusType {
    return this.getCurrentLineFocusStyle().type;
  }

  getCurrentLineFocusStyle(): LineFocusStyle {
    return this.model_.getCurrentLineFocusStyle();
  }

  getCurrentLineFocusMovement(): LineFocusMovement {
    return this.model_.getCurrentLineFocusMovement();
  }

  // Whether the current line focus mode is static.
  isStatic(): boolean {
    return this.model_.getCurrentLineFocusMovement() ===
        LineFocusMovement.STATIC;
  }

  addListener(listener: LineFocusListener) {
    this.listeners_.push(listener);
  }

  isEnabled(): boolean {
    return (
        chrome.readingMode.isLineFocusEnabled &&
        this.getCurrentLineFocusType() !== LineFocusType.NONE);
  }

  toggle(container: HTMLElement, height: number) {
    if (!chrome.readingMode.isLineFocusEnabled) {
      return;
    }

    const lastStyle = this.model_.getLastEnabledLineFocusStyle();
    const newStyle = this.isEnabled() ? LineFocusStyle.OFF : lastStyle;
    this.setStyleAndMovement_(
        newStyle, this.model_.getCurrentLineFocusMovement(), container, height);
    this.logger_.logLineFocusToggled(this.isEnabled());
    this.listeners_.forEach(l => l.onLineFocusToggled());
  }

  onScrollEnd(newScrollTop: number) {
    if (this.isEnabled()) {
      const distance =
          Math.round(Math.abs(newScrollTop - this.model_.getLastScrollTop()));
      chrome.readingMode.addLineFocusScrollDistance(distance);
      this.model_.setLastScrollTop(newScrollTop);

      if (this.model_.getInitiatedScroll()) {
        this.model_.setInitiatedScroll(false);
        if (this.isStatic() && this.getCurrentLineFocusLines_() > 1) {
          return;
        }
        const oldHeight = this.getHeight();
        const oldTop = this.getTop();
        this.calculateHeight_();
        const heightDiff = (oldHeight === null || this.getHeight() === null) ?
            null :
            Math.abs(oldHeight - this.getHeight()!);
        const topDiff = Math.abs(oldTop - this.getTop());
        if (heightDiff === null || heightDiff > WINDOW_DIFF_THRESHOLD ||
            topDiff > WINDOW_DIFF_THRESHOLD) {
          this.listeners_.forEach(l => l.onLineFocusMove());
        }
      } else {
        // If the scroll is user-initiated then reset the line index for the
        // purpose of line-by-line keyboard movement.
        this.model_.setCurrentLineIndex(null);
      }
    }
  }

  onMouseMove(y: number) {
    // Line focus should follow along with speech if it's active, so ignore
    // mouse movements.
    if (this.isEnabled() && !this.speechController_.isSpeechActive() &&
        !this.isStatic()) {
      this.model_.setCurrentLineIndex(null);
      const previousY = this.model_.getY();
      this.setY_(Math.max(this.model_.getMinY(), y));
      chrome.readingMode.addLineFocusMouseDistance(
          Math.round(Math.abs(this.model_.getY() - previousY)));
    }
  }

  onMouseMoveInToolbar(y: number) {
    if (this.isEnabled() && !this.speechController_.isSpeechActive() &&
        !this.isStatic()) {
      // Store the new position, but do not notify listeners since the mouse is
      // in the toolbar, which means they are likely trying to change some
      // settings. onAllMenusClose will notify them of the final position when
      // all the settings menus are closed.
      this.setY_(Math.max(this.model_.getMinY(), y), /* quietly= */ true);
    }
  }

  onAllMenusClose() {
    this.listeners_.forEach(l => l.onLineFocusMove());
  }

  onWordBoundary(segments: Segment[]) {
    if (!this.isEnabled()) {
      return;
    }

    const rects: DOMRect[] = [];
    for (const {node, start} of segments) {
      const domNode = node.domNode();
      if (!domNode) {
        continue;
      }
      const {node: finalNode, offset} = getTextNodeOffsets(domNode, start);
      const startOffset = start - offset;
      const range = document.createRange();

      range.setStart(finalNode, startOffset);
      range.setEndAfter(finalNode);
      rects.push(...Array.from(range.getClientRects()));
    }

    const sortedRects =
        Array.from(new Set(rects)).sort((a, b) => a.bottom - b.bottom);
    if (!sortedRects.length) {
      return;
    }

    if (this.model_.getY() !== this.getNewY_(sortedRects[0]!)) {
      chrome.readingMode.incrementLineFocusSpeechLines();
    }
    this.setyOrScroll_(sortedRects[0]!);

    // If line focus would go off screen, scroll the text to the center.
    const height = this.getHeight() || 0;
    const bottom = this.getTop() + height;
    if (bottom > this.model_.getMaxY()) {
      this.scroll_(bottom - (this.model_.getMaxY() / 2));
    } else if (this.getTop() < this.model_.getMinY()) {
      this.scroll_(
          this.model_.getMinY() + this.model_.getY() -
          (this.model_.getMaxY() / 2));
    }
  }

  onTextLocationsChange(container: HTMLElement, height: number) {
    if (this.isEnabled()) {
      const previousMaxY = this.model_.getMaxY();
      const previousMinY = this.model_.getMinY();
      this.calculateNewPositions_(container, height);
      if (this.isStatic()) {
        if (previousMaxY !== this.model_.getMaxY() ||
            previousMinY !== this.model_.getMinY()) {
          this.setCenterY_();
        }
        return;
      }

      this.setY_(this.model_.getY());
    }
  }

  restoreFromPrefs(
      lastEnabledValue: number, isOn: boolean, container: HTMLElement,
      height: number) {
    const lineFocusValues = getLineFocusValues();
    const lastEnabled = lineFocusValues[lastEnabledValue];
    if (lastEnabled) {
      this.model_.setLastEnabledLineFocusStyle(lastEnabled.style);
      const style = isOn ? lastEnabled.style : LineFocusStyle.OFF;
      this.setStyleAndMovement_(style, lastEnabled.movement, container, height);
    }
  }

  onStyleChange(style: LineFocusStyle, container: HTMLElement, height: number) {
    this.setStyleAndMovement_(
        style, this.model_.getCurrentLineFocusMovement(), container, height);
  }

  onMovementChange(
      movement: LineFocusMovement, container: HTMLElement, height: number) {
    this.setStyleAndMovement_(
        this.getCurrentLineFocusStyle(), movement, container, height);
  }

  private setStyleAndMovement_(
      style: LineFocusStyle, movement: LineFocusMovement,
      container: HTMLElement, height: number) {
    const wasEnabled = this.isEnabled();
    this.model_.setCurrentLineFocusStyle(style);
    this.model_.setCurrentLineFocusMovement(movement);
    this.propagateLineFocus_(style, movement);
    const isOff = style === LineFocusStyle.OFF;
    if (!isOff) {
      this.model_.setLastEnabledLineFocusStyle(style);
    }
    this.updateLineFocus_(isOff, wasEnabled, container, height);
  }

  private updateLineFocus_(
      isOff: boolean, wasEnabled: boolean, container: HTMLElement,
      height: number) {
    if (isOff) {
      this.logger_.logLineFocusSession();
      this.model_.setMinY(0);
      this.model_.setMaxY(0);
      this.model_.setY(0);
      this.model_.setTop(0);
      this.model_.setWindowHeight(0);
      this.model_.setTextBounds([]);
      this.model_.setCurrentLineIndex(null);
      this.model_.setLastScrollTop(0);
    } else {
      // This is the start of a line focus session if it was off before this.
      if (!wasEnabled) {
        chrome.readingMode.startLineFocusSession();
      }
      this.calculateNewPositions_(container, height);
      if (this.isStatic()) {
        this.setCenterY_();
      } else {
        this.setY_(Math.max(this.model_.getMinY(), this.model_.getY()));
      }
    }
  }

  private propagateLineFocus_(
      style: LineFocusStyle, movement: LineFocusMovement) {
    if (!chrome.readingMode.isLineFocusEnabled) {
      return;
    }
    const lineFocusValue = this.lineFocusToEnumValue_(style, movement);
    if (lineFocusValue !== null) {
      chrome.readingMode.onLineFocusChanged(lineFocusValue);
    }
  }

  private lineFocusToEnumValue_(
      style: LineFocusStyle, movement: LineFocusMovement): number|null {
    if (style === LineFocusStyle.OFF) {
      return chrome.readingMode.lineFocusOff;
    }
    const lineFocusValues = getLineFocusValues();
    const key = Object.keys(lineFocusValues).find(key => {
      const lineFocus = lineFocusValues[Number(key)];
      return lineFocus?.style === style && lineFocus?.movement === movement;
    });
    return key ? Number(key) : null;
  }

  snapToNextLine(isForward: boolean) {
    if (!this.isEnabled() || this.speechController_.isSpeechActive()) {
      return;
    }

    const lines = this.model_.getTextBounds();
    if (!lines.length) {
      return;
    }

    // If this is the first time snapping after mouse movement, move to the
    // closest line to the current Y.
    const currentIndex = this.model_.getCurrentLineIndex();
    if (currentIndex === null) {
      this.initializeSnapIndex_(lines, isForward);
      return;
    }

    this.updateSnapIndex_(lines, currentIndex, isForward);
  }

  private initializeSnapIndex_(lines: DOMRect[], isForward: boolean) {
    const rawIndex = this.getFirstLineIndex_(isForward);
    const safeIndex = this.clampLineIndex_(rawIndex);

    this.model_.setCurrentLineIndex(safeIndex);
    this.setyOrScroll_(lines[safeIndex]!);

    const linesToLog = this.getCurrentLineFocusLines_();
    for (let i = 0; i < linesToLog; i++) {
      chrome.readingMode.incrementLineFocusKeyboardLines();
    }
  }

  private updateSnapIndex_(
      lines: DOMRect[], currentIndex: number, isForward: boolean) {
    const direction = isForward ? 1 : -1;
    const nextIndex = currentIndex + direction;
    const bottomIndex =
        nextIndex + ((this.getCurrentLineFocusLines_() - 1) / 2);

    if (nextIndex < 0 || bottomIndex >= lines.length) {
      return;
    }

    const clampedIndex = this.clampLineIndex_(nextIndex);
    this.model_.setCurrentLineIndex(clampedIndex);

    // Calculate visibility bounds to see if we need to scroll.
    const {topRect, bottomRect} = this.getFocusWindowRects_(lines, nextIndex);

    const isOutOfView = bottomRect.bottom > this.model_.getMaxY() ||
        topRect.top < this.model_.getMinY();

    if (isOutOfView) {
      // Scroll the container to keep line focus in view if it would go out of
      // view.
      chrome.readingMode.incrementLineFocusKeyboardLines();
      // TODO(crbug.com/447427066): Consider whether to instead scroll one
      // line at a time. If so, uncomment the code below and remove the center
      // logic below.
      // const scrollDiff = lines[nextIndex]! - lines[clampedIndex]!;

      // Center it vertically.
      const desiredCenter =
          this.getCurrentLineFocusType() === LineFocusType.LINE ?
          bottomRect.bottom :
          (topRect.top + bottomRect.bottom) / 2;
      const scrollDiff = desiredCenter - (this.model_.getMaxY() / 2);
      this.scroll_(scrollDiff);
    } else if (this.model_.getCurrentLineIndex() !== currentIndex) {
      chrome.readingMode.incrementLineFocusKeyboardLines();
      this.setyOrScroll_(lines[nextIndex]!);
    }

    // If the user has navigated back to the top of the panel, but there's
    // still a little bit left to scroll, scroll to the top.
    if (this.model_.getCurrentLineIndex() === currentIndex) {
      this.listeners_.forEach(l => l.onNeedScrollToTop());
    }
  }

  // Gets the DOMRects for the top and bottom of the focus window for a given
  // center line index
  private getFocusWindowRects_(lines: DOMRect[], targetIndex: number) {
    const numLines = this.getCurrentLineFocusLines_();
    const isLineMode = this.getCurrentLineFocusType() === LineFocusType.LINE;

    // In Line Mode, the "window" is just the line itself.
    // In Window Mode, the "window" spans multiple lines around the center.
    const topIndex =
        isLineMode ? targetIndex : Math.max(0, targetIndex - (numLines / 2));
    const bottomIndex = isLineMode ?
        targetIndex :
        Math.min(lines.length - 1, topIndex + numLines);

    return {
      topRect: lines[Math.floor(topIndex)]!,
      bottomRect: lines[Math.floor(bottomIndex)] || lines[lines.length - 1]!,
    };
  }

  private getCurrentLineFocusLines_(): number {
    return this.getCurrentLineFocusStyle().lines;
  }

  // If line focus is a window of > 1 line, the bottom of the window should not
  // go above the number of lines in the window.
  private clampLineIndex_(index: number): number {
    return this.getCurrentLineFocusType() === LineFocusType.LINE ?
        index :
        Math.max(index, (this.getCurrentLineFocusLines_() - 1) / 2);
  }

  private getNewY_(newBounds: DOMRect) {
    return this.getCurrentLineFocusType() === LineFocusType.LINE ?
        newBounds.bottom :
        (newBounds.top + newBounds.bottom) / 2;
  }

  // When the current line focus mode is static, scroll the content instead of
  // moving the line focus element.
  private setyOrScroll_(newBounds: DOMRect) {
    const newY = this.getNewY_(newBounds);
    if (this.isStatic()) {
      const scrollDiff = newY - this.model_.getY();
      this.scroll_(scrollDiff);
    } else {
      this.setY_(newY);
    }
  }

  private scroll_(scrollDiff: number) {
    if (Math.abs(scrollDiff) < SCROLL_THRESHOLD) {
      return;
    }
    this.model_.setInitiatedScroll(true);
    this.listeners_.forEach(l => l.onNeedScrollForLineFocus(scrollDiff));
  }

  private setY_(y: number, quietly: boolean = false) {
    this.model_.setY(y);
    this.calculateHeight_();
    if (!quietly) {
      this.listeners_.forEach(l => l.onLineFocusMove());
    }
  }

  private calculateHeight_() {
    if (this.getCurrentLineFocusType() === LineFocusType.LINE) {
      this.model_.setTop(this.model_.getY());
      return;
    }

    // In window mode, always use the calculated line locations to set the top
    // and height of the window.
    const bounds = this.model_.getTextBounds();
    if (bounds.length === 0) {
      return;
    }

    const currentLineIndex =
        this.model_.getCurrentLineIndex() || this.getFirstLineIndex_(true);

    const numLines = this.getCurrentLineFocusStyle().lines;
    const topIndex = currentLineIndex - ((numLines - 1) / 2);
    const maxTopIndex = bounds.length - numLines;
    const minTopIndex =
        bounds.findIndex(rect => rect.top >= this.model_.getMinY());
    const validTopIndex =
        Math.max(minTopIndex, Math.min(maxTopIndex, topIndex));
    const topLine = bounds[validTopIndex]!;
    this.model_.setTop(topLine.top);
    const bottomIndex = (validTopIndex + numLines - 1);
    const bottom = bottomIndex < bounds.length ? bounds[bottomIndex]!.bottom :
                                                 this.model_.getMaxY();
    this.model_.setWindowHeight(bottom - this.getTop());
  }

  private calculateNewPositions_(container: HTMLElement, height: number) {
    const currentLineFocus = this.getCurrentLineFocusStyle();
    assert(!!currentLineFocus);
    this.model_.setMinY(container.offsetTop);
    this.model_.setMaxY(this.model_.getMinY() + height);

    const range = document.createRange();
    range.selectNodeContents(container);

    const newBounds =
        this.combineIntersectingRects_(Array.from(range.getClientRects()));
    this.model_.setTextBounds(newBounds);

    // Adjust line focus to remain at the same text line even if it's moved,
    // due to font or other spacing changes.
    const newLines = newBounds.map(rect => rect.bottom);
    const currentLineIndex = this.model_.getCurrentLineIndex();
    if (!this.isStatic() && currentLineIndex && currentLineIndex >= 0 &&
        currentLineIndex < newLines.length - 1) {
      this.setY_(newLines[currentLineIndex]!);
    }
  }

  private combineIntersectingRects_(unsortedRects: DOMRect[]): DOMRect[] {
    if (unsortedRects.length === 0) {
      return [];
    }

    const sortedRects =
        Array.from(new Set(unsortedRects)).sort((a, b) => a.bottom - b.bottom);
    const combinedRects: DOMRect[] = [sortedRects[0]!];
    // The smaller the line spacing, the larger the threshold needs to be, since
    // it is more likely for lines to have overlapping bounds. Thus, invert the
    // line spacing value and multiply by 10 to ensure it is above 1.
    const lineHeight =
        chrome.readingMode.getLineSpacingValue(chrome.readingMode.lineSpacing);
    const threshold =
        Math.max(1, chrome.readingMode.fontSize) * (1 / lineHeight) * 10;
    for (let i = 1; i < sortedRects.length; i++) {
      const currentRect = sortedRects[i]!;
      const lastRect = combinedRects[combinedRects.length - 1]!;

      // The rects are sorted by their bottom values. If the current rect top is
      // above the previous rect top, then it encompasses the previous line (or
      // more), so this rect is not a single line of text.
      if (currentRect.top < lastRect.top) {
        continue;
      }

      // Skip duplicate rects.
      if (lastRect.top === currentRect.top &&
          lastRect.bottom === currentRect.bottom) {
        continue;
      }

      // If the next rect intersects with the last rect, and the intersection is
      // larger than a threshold, merge them by removing the last rect and
      // keeping the new one with a higher bottom value. The threshold is > 0
      // because some fonts may cause their returned rects to slightly overlap,
      // even though the lines are visually distinct.
      const isIntersecting = lastRect.bottom > currentRect.top &&
          lastRect.bottom <= currentRect.bottom;
      if (isIntersecting && (lastRect.bottom - currentRect.top) > threshold) {
        combinedRects.pop();
      }
      combinedRects.push(currentRect);
    }

    return combinedRects;
  }

  // Returns the closest line index based on the current Y position.
  private getFirstLineIndex_(isForward: boolean): number {
    let previousY = 0;
    const lines = this.model_.getTextBounds();
    for (let index = 0; index < lines.length; index++) {
      const line = lines[index]!.bottom;
      if (this.model_.getY() >= previousY && this.model_.getY() < line) {
        return (isForward || (index <= 0)) ? index : index - 1;
      }
      previousY = line;
    }

    return lines.length - 1;
  }

  private setCenterY_() {
    this.setY_((this.model_.getMinY() + this.model_.getMaxY()) / 2);
  }

  static getInstance(): LineFocusController {
    return instance || (instance = new LineFocusController());
  }

  static setInstance(obj: LineFocusController) {
    instance = obj;
  }
}

let instance: LineFocusController|null = null;
