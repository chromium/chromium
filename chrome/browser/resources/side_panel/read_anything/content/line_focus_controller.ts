// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';

import {LineFocus, LineFocusType} from '../content/read_anything_types.js';
import {currentReadHighlightClass, PARENT_OF_HIGHLIGHT_CLASS} from '../read_aloud/movement.js';
import {SpeechController} from '../read_aloud/speech_controller.js';

import {LineFocusModel} from './line_focus_model.js';

export interface LineFocusListener {
  onLineFocusMove(): void;
  onNeedScrollForLineFocus(scrollDiff: number): void;
  onNeedScrollToTop(): void;
}

// Handles the business logic for managing the line focus feature.
export class LineFocusController {
  private readonly listeners_: LineFocusListener[] = [];
  private model_: LineFocusModel = new LineFocusModel();
  private highlightObserver_: MutationObserver;
  private speechController_ = SpeechController.getInstance();

  constructor() {
    this.highlightObserver_ = new MutationObserver(this.onMutation_.bind(this));
  }

  getTop(): number {
    return this.model_.getTop();
  }

  getHeight(): number|null {
    return (this.getCurrentLineFocusType() === LineFocusType.WINDOW) ?
        this.model_.getWindowHeight() :
        null;
  }

  getCurrentLineFocusType(): LineFocusType|undefined {
    return this.model_.getCurrentLineFocus()?.type;
  }

  addListener(listener: LineFocusListener) {
    this.listeners_.push(listener);
  }

  isEnabled(): boolean {
    return (
        chrome.readingMode.isLineFocusEnabled &&
        !!this.model_.getCurrentLineFocus() &&
        (this.model_.getCurrentLineFocus() !== LineFocus.OFF));
  }

  onMouseMove(y: number) {
    // Line focus should follow along with speech if it's active, so ignore
    // mouse movements.
    if (this.isEnabled() && !this.speechController_.isSpeechActive() &&
        !this.isStatic_()) {
      this.model_.setCurrentLineIndex(null);
      this.setY_(Math.max(this.model_.getMinY(), y));
    }
  }

  onTextLocationsChange(container: HTMLElement, height: number) {
    if (this.isEnabled()) {
      this.calculateNewPositions_(container, height);
      if (this.isStatic_()) {
        return;
      }
      if (this.speechController_.isSpeechActive()) {
        const highlights = container.querySelectorAll<HTMLElement>(
            `.${currentReadHighlightClass}`);
        this.moveBelowHighlights_(Array.from(highlights));
      } else {
        this.setY_(this.model_.getY());
      }
    }
  }

  onLineFocusChange(
      lineFocus: LineFocus, container: HTMLElement, height: number) {
    this.model_.setCurrentLineFocus(lineFocus);
    if (lineFocus.type === LineFocusType.NONE) {
      this.model_.setMinY(0);
      this.model_.setMaxY(0);
      this.model_.setY(0);
      this.model_.setTop(0);
      this.model_.setWindowHeight(0);
      this.model_.setDefaultWindowHeight(0);
      this.model_.setTextLineBottoms([]);
      this.model_.setCurrentLineIndex(null);
      this.highlightObserver_.disconnect();
    } else {
      this.calculateNewPositions_(container, height);
      if (this.isStatic_()) {
        // Set the static line in the center of the visible area.
        this.setY_(this.model_.getMinY() + this.model_.getMaxY() / 2);
      } else {
        this.setY_(Math.max(this.model_.getMinY(), this.model_.getY()));
      }
    }
  }

  snapToNextLine(isForward: boolean) {
    if (!this.isEnabled() || this.speechController_.isSpeechActive()) {
      return;
    }

    const lines = this.model_.getTextLineBottoms();
    if (!lines.length) {
      return;
    }

    // If this is the first time snapping after mouse movement, move to the
    // closest line to the current Y.
    const currentLineIndex = this.model_.getCurrentLineIndex();
    if (currentLineIndex === null) {
      const firstIndex =
          this.clampLineIndex_(this.getFirstLineIndex_(isForward));
      this.model_.setCurrentLineIndex(firstIndex);
      this.setyOrScroll_(lines[firstIndex]!);
      return;
    }

    const diff = isForward ? 1 : -1;
    const previousLineIndex = currentLineIndex;
    const newIndex = currentLineIndex + diff;
    if (newIndex >= 0 && newIndex < lines.length) {
      this.model_.setCurrentLineIndex(this.clampLineIndex_(newIndex));
      const lineFocusTop =
          this.getCurrentLineFocusType() === LineFocusType.LINE ?
          lines[newIndex]! :
          lines[newIndex - this.getCurrentLineFocusLines_()]!;
      // Scroll the container to keep line focus in view if it would go out of
      // view.
      if (lines[newIndex]! > this.model_.getMaxY() ||
          lineFocusTop < this.model_.getMinY()) {
        // TODO(crbug.com/447427066): Consider whether to instead scroll one
        // line at a time. If so, uncomment the code below and remove the center
        // logic below.
        // const scrollDiff = lines[newIndex]! - lines[currentLineIndex]!;

        // Center it vertically.
        const scrollDiff = (lines.at(newIndex)! - (this.model_.getMaxY() / 2));
        this.listeners_.forEach(l => l.onNeedScrollForLineFocus(scrollDiff));
      } else if (this.model_.getCurrentLineIndex() !== previousLineIndex) {
        this.setyOrScroll_(lines[newIndex]!);
      }

      // If the user has navigated back to the top of the panel, but there's
      // still a little bit left to scroll, scroll to the top.
      if (this.model_.getCurrentLineIndex() === previousLineIndex) {
        this.listeners_.forEach(l => l.onNeedScrollToTop());
      }
    }
  }

  private getCurrentLineFocusLines_(): number {
    return this.model_.getCurrentLineFocus() ?
        this.model_.getCurrentLineFocus()!.lines :
        0;
  }


  // If line focus is a window of > 1 line, the bottom of the window should not
  // go above the number of lines in the window.
  private clampLineIndex_(index: number): number {
    return this.getCurrentLineFocusType() === LineFocusType.LINE ?
        index :
        Math.max(index, this.getCurrentLineFocusLines_() - 1);
  }

  private isStatic_(): boolean {
    return this.model_.getCurrentLineFocus() === LineFocus.STATIC_LINE;
  }

  // When the current line focus mode is static, scroll the content instead of
  // moving the line focus element.
  private setyOrScroll_(newY: number) {
    if (this.isStatic_()) {
      const scrollDiff = newY - this.model_.getY();
      this.listeners_.forEach(l => l.onNeedScrollForLineFocus(scrollDiff));
    } else {
      this.setY_(newY);
    }
  }

  private setY_(y: number) {
    const oldY = this.model_.getY();
    this.model_.setY(y);

    const oldHeight = this.model_.getWindowHeight();
    this.calculateHeight_();

    if (oldY === this.model_.getY() &&
        oldHeight === this.model_.getWindowHeight()) {
      return;
    }

    this.listeners_.forEach(l => l.onLineFocusMove());
  }

  private calculateHeight_() {
    const currentLineFocus = this.model_.getCurrentLineFocus();
    if (!currentLineFocus) {
      return;
    }

    if (this.getCurrentLineFocusType() === LineFocusType.LINE) {
      this.model_.setTop(this.model_.getY());
      return;
    }

    // If the line focus is a window being controlled with smooth mouse movement
    // then use the default window height.
    const currentLineIndex = this.model_.getCurrentLineIndex();
    if (currentLineIndex === null) {
      this.model_.setWindowHeight(this.model_.getDefaultWindowHeight());
      this.model_.setTop(Math.max(
          this.model_.getMinY(),
          this.model_.getY() - this.model_.getWindowHeight()));
      return;
    }

    // If the line focus is a window being controlled with discrete keyboard
    // presses, then use the calculated line locations to set the top and height
    // of the window.
    const topIndex = currentLineIndex - currentLineFocus.lines;
    const index = Math.max(
        0, Math.min(this.model_.getTextLineBottoms().length - 1, topIndex));
    const shouldUseMinY = topIndex < 0 ||
        this.model_.getTextLineBottoms()[index]! < this.model_.getMinY();
    this.model_.setTop(
        shouldUseMinY ? this.model_.getMinY() :
                        this.model_.getTextLineBottoms()[index]!);
    if (!shouldUseMinY || topIndex === -1) {
      this.model_.setWindowHeight(
          this.model_.getTextLineBottoms()[currentLineIndex]! - this.getTop());
    }
  }

  private calculateNewPositions_(container: HTMLElement, height: number) {
    const currentLineFocus = this.model_.getCurrentLineFocus();
    assert(!!currentLineFocus);
    this.model_.setMinY(container.offsetTop);
    this.model_.setMaxY(this.model_.getMinY() + height);

    const range = document.createRange();
    range.selectNodeContents(container);

    const newLines =
        this.combineIntersectingRects_(Array.from(range.getClientRects()))
            .map(rect => rect.bottom);
    this.model_.setTextLineBottoms(newLines);
    const visibleLines = newLines.filter(
        y => y >= this.model_.getMinY() && y <= this.model_.getMaxY());
    this.model_.setDefaultWindowHeight(
        currentLineFocus.lines * (visibleLines.at(-1)! - visibleLines.at(0)!) /
        (visibleLines.length - 1));

    // Adjust line focus to remain at the same text line even if it's moved,
    // due to font or other spacing changes.
    const currentLineIndex = this.model_.getCurrentLineIndex();
    if (!this.isStatic_() && currentLineIndex && currentLineIndex >= 0 &&
        currentLineIndex < newLines.length - 1) {
      this.setY_(newLines[currentLineIndex]!);
    }

    this.highlightObserver_.disconnect();
    // Listen for node additions because speech is highlighted by replacing a
    // node with its parts split into multiple nodes and styled differently.
    this.highlightObserver_.observe(container, {
      childList: true,
      subtree: true,
    });
  }

  private onMutation_(mutations: MutationRecord[]) {
    if (!this.isEnabled()) {
      return;
    }

    // Extract the current highlights from the mutations.
    const isHighlightParent = (node: Node): node is HTMLElement =>
        node instanceof HTMLElement &&
        node.classList.contains(PARENT_OF_HIGHLIGHT_CLASS);
    const getCurrentHighlights = (el: HTMLElement): HTMLElement[] => Array.from(
        el.querySelectorAll<HTMLElement>(`.${currentReadHighlightClass}`));
    const highlights = mutations.flatMap(m => Array.from(m.addedNodes))
                           .filter(isHighlightParent)
                           .flatMap(getCurrentHighlights);
    this.moveBelowHighlights_(highlights);
  }

  private moveBelowHighlights_(highlights: HTMLElement[]) {
    if (highlights.length > 0) {
      const maxY =
          Math.max(...highlights.map(h => h.getBoundingClientRect().bottom));
      this.setyOrScroll_(maxY);
    }
  }

  private combineIntersectingRects_(unsortedRects: DOMRect[]): DOMRect[] {
    if (unsortedRects.length === 0) {
      return [];
    }

    const sortedRects =
        Array.from(new Set(unsortedRects)).sort((a, b) => a.bottom - b.bottom);
    const combinedRects: DOMRect[] = [sortedRects[0]!];
    for (let i = 1; i < sortedRects.length; i++) {
      const currentRect = sortedRects[i]!;
      const lastRect = combinedRects[combinedRects.length - 1]!;

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
      const threshold = Math.max(1, chrome.readingMode.fontSize);
      if (isIntersecting && (lastRect.bottom - currentRect.top) > threshold) {
        combinedRects.pop();
      }
      combinedRects.push(currentRect);
    }

    return combinedRects;
  }

  // Returns the closest line index based on the current Y position.
  private getFirstLineIndex_(isForward: boolean): number {
    let previousLine = 0;
    const lines = this.model_.getTextLineBottoms();
    for (let index = 0; index < lines.length; index++) {
      const line = lines[index]!;
      if (this.model_.getY() >= previousLine && this.model_.getY() < line) {
        return (isForward || (index <= 0)) ? index : index - 1;
      }
      previousLine = line;
    }

    return 0;
  }

  static getInstance(): LineFocusController {
    return instance || (instance = new LineFocusController());
  }

  static setInstance(obj: LineFocusController) {
    instance = obj;
  }
}

let instance: LineFocusController|null = null;
