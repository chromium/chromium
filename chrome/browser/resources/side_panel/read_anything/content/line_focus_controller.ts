// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {getLineFocusValues, LineFocusMovement, LineFocusStyle, LineFocusType} from '../content/read_anything_types.js';
import type {Segment} from '../read_aloud/read_aloud_types.js';
import {SpeechController} from '../read_aloud/speech_controller.js';
import {getRectIndexAtY, getRectsForSegments} from '../shared/dom_queries.js';
import {isForwardArrow, isLineFocusShortcut, isVerticalArrow} from '../shared/keyboard_util.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {LineFocusModel} from './line_focus_model.js';
import {LineFocusCursorMoveMode, LineFocusNoneMoveMode, LineFocusStaticMoveMode} from './line_focus_move_mode.js';
import type {MoveModeDelegate} from './line_focus_move_mode.js';
import {LineFocusLineStyleMode, LineFocusNoneStyleMode, LineFocusWindowStyleMode} from './line_focus_style_mode.js';

export interface LineFocusListener {
  onLineFocusMove(): void;
  onNeedScrollForLineFocus(scrollDiff: number, instant?: boolean): void;
  onNeedScrollToTop(): void;
  onLineFocusToggled(): void;
}

// Handles the business logic for managing the line focus feature.
export class LineFocusController implements MoveModeDelegate {
  private readonly listeners_: LineFocusListener[] = [];
  private model_: LineFocusModel = new LineFocusModel();
  private speechController_ = SpeechController.getInstance();
  private logger_ = ReadAnythingLogger.getInstance();

  constructor() {
    const styleMode =
        new LineFocusNoneStyleMode(LineFocusStyle.OFF, this.model_);
    this.model_.setCurrentStyleMode(styleMode);
    this.model_.setCurrentMoveMode(new LineFocusNoneMoveMode(
        this.model_, styleMode, this, LineFocusMovement.STATIC));
  }

  getTop(): number {
    return this.model_.getTop();
  }

  getHeight(): number|null {
    return (this.getCurrentLineFocusType() === LineFocusType.WINDOW) ?
        this.model_.getWindowHeight() :
        null;
  }

  getCurrentLineFocusType(): LineFocusType {
    return this.model_.getCurrentStyleMode().getStyle().type;
  }

  getCurrentLineFocusStyle(): LineFocusStyle {
    return this.model_.getCurrentStyleMode().getStyle();
  }

  getCurrentLineFocusMovement(): LineFocusMovement {
    return this.model_.getCurrentMoveMode().getMovement();
  }

  // Whether the current line focus mode is static.
  isStatic(): boolean {
    return this.getCurrentLineFocusMovement() === LineFocusMovement.STATIC;
  }

  addListener(listener: LineFocusListener) {
    this.listeners_.push(listener);
  }

  isEnabled(): boolean {
    return (
        chrome.readingMode.isLineFocusEnabled &&
        this.getCurrentLineFocusType() !== LineFocusType.NONE);
  }

  private toggle_(container: HTMLElement, height: number) {
    if (!chrome.readingMode.isLineFocusEnabled) {
      return;
    }

    const lastStyle = this.model_.getLastEnabledLineFocusStyle();
    const newStyle = this.isEnabled() ? LineFocusStyle.OFF : lastStyle;
    this.setStyleAndMovement_(
        newStyle, this.getCurrentLineFocusMovement(), container, height);
    this.logger_.logLineFocusToggled(this.isEnabled());
    this.listeners_.forEach(l => l.onLineFocusToggled());
  }

  onKeyDown(e: KeyboardEvent, container: HTMLElement, height: number): boolean {
    if (!chrome.readingMode.isLineFocusEnabled) {
      return false;
    }

    if (isLineFocusShortcut(e)) {
      this.toggle_(container, height);
      return true;
    }

    if (this.isEnabled() && isVerticalArrow(e.key) &&
        !this.speechController_.isSpeechActive()) {
      this.snapToNextLine_(isForwardArrow(e.key));
      return true;
    }

    return false;
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
        const oldHeight = this.model_.getWindowHeight();
        const oldTop = this.getTop();
        this.model_.getCurrentStyleMode().calculateHeight();
        if (this.model_.getCurrentStyleMode().shouldRefreshFocalPoint(
                oldHeight, oldTop)) {
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
      const previousY = this.model_.getFocalPoint();
      this.model_.getCurrentMoveMode().setFocalPoint(
          Math.max(this.model_.getMinY(), y));
      chrome.readingMode.addLineFocusMouseDistance(
          Math.round(Math.abs(this.model_.getFocalPoint() - previousY)));
    }
  }

  onMouseMoveInToolbar(y: number) {
    if (this.isEnabled() && !this.speechController_.isSpeechActive() &&
        !this.isStatic()) {
      // Store the new position, but do not notify listeners since the mouse is
      // in the toolbar, which means they are likely trying to change some
      // settings. onAllMenusClose will notify them of the final position when
      // all the settings menus are closed.
      this.model_.getCurrentMoveMode().setFocalPoint(
          Math.max(this.model_.getMinY(), y), /* quietly= */ true);
    }
  }

  onAllMenusClose() {
    this.listeners_.forEach(l => l.onLineFocusMove());
  }

  onWordBoundary(segments: Segment[]) {
    if (!this.isEnabled()) {
      return;
    }

    const sortedRects = getRectsForSegments(segments);
    if (!sortedRects.length) {
      return;
    }

    if (this.model_.getFocalPoint() !==
        this.model_.getCurrentStyleMode().getFocalPointForRect(
            sortedRects[0]!)) {
      chrome.readingMode.incrementLineFocusSpeechLines();
    }
    this.model_.getCurrentMoveMode().moveToRect(sortedRects[0]!);

    // If line focus would go off screen, scroll the text to the center.
    const height = this.getHeight() || 0;
    const bottom = this.getTop() + height;
    if (bottom > this.model_.getMaxY()) {
      this.model_.getCurrentMoveMode().scroll(
          bottom - (this.model_.getMaxY() / 2));
    } else if (this.getTop() < this.model_.getMinY()) {
      this.model_.getCurrentMoveMode().scroll(
          this.model_.getMinY() + this.model_.getFocalPoint() -
          (this.model_.getMaxY() / 2));
    }
  }

  onTextLocationsChange(container: HTMLElement, height: number) {
    if (this.isEnabled()) {
      const previousMaxY = this.model_.getMaxY();
      const previousMinY = this.model_.getMinY();

      // Save the current line index before recalculating positions so we know
      // which line was in focus.
      const currentIndex = this.model_.getCurrentLineIndex() ??
          getRectIndexAtY(this.model_.getFocalPoint(),
                          this.model_.getTextBounds(),
                          /*isForward=*/ true);
      this.model_.setCurrentLineIndex(currentIndex);

      this.calculateNewPositions_(container, height);

      const lines = this.model_.getTextBounds();
      if (lines.length > 0) {
        const targetIndex =
            Math.max(0, Math.min(currentIndex, lines.length - 1));
        const clampedIndex =
            this.model_.getCurrentStyleMode().clampLineIndex(targetIndex);
        this.model_.setCurrentLineIndex(clampedIndex);

        // Re-center the line that's in focus when text locations change in
        // cursor mode. Scroll instantly to reduce dizzying movement.
        if (!this.isStatic()) {
          this.model_.getCurrentMoveMode().scrollToCenter(
              lines, clampedIndex, /*instant=*/ true);
        }
      }

      if (this.isStatic()) {
        if (previousMaxY !== this.model_.getMaxY() ||
            previousMinY !== this.model_.getMinY()) {
          this.setCenterY_();
        }
        return;
      }

      this.model_.getCurrentMoveMode().setFocalPoint(
          this.model_.getFocalPoint());
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
        style, this.getCurrentLineFocusMovement(), container, height);
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
    this.updateStrategies_(style, movement);
    this.propagateLineFocus_(style, movement);
    this.model_.getCurrentMoveMode().onActivated(container, height);
    if (style !== LineFocusStyle.OFF) {
      this.updateLineFocus_(wasEnabled, container, height);
    }
  }

  private updateStrategies_(
      style: LineFocusStyle, movement: LineFocusMovement) {
    if (style.type === LineFocusType.NONE) {
      const styleMode = new LineFocusNoneStyleMode(style, this.model_);
      this.model_.setCurrentStyleMode(styleMode);
      this.model_.setCurrentMoveMode(
          new LineFocusNoneMoveMode(this.model_, styleMode, this, movement));
      return;
    }

    const styleMode = style.type === LineFocusType.LINE ?
        new LineFocusLineStyleMode(style, this.model_) :
        new LineFocusWindowStyleMode(style, this.model_);
    this.model_.setCurrentStyleMode(styleMode);

    const moveMode = movement === LineFocusMovement.STATIC ?
        new LineFocusStaticMoveMode(this.model_, styleMode, this) :
        new LineFocusCursorMoveMode(this.model_, styleMode, this);
    this.model_.setCurrentMoveMode(moveMode);
  }

  private updateLineFocus_(
      wasEnabled: boolean, container: HTMLElement, height: number) {
    this.calculateNewPositions_(container, height);
    if (this.isStatic()) {
      this.setCenterY_();
    } else if (!wasEnabled && this.model_.getTextBounds().length > 0) {
      this.model_.getCurrentMoveMode().initializeSnapIndex(
          this.model_.getTextBounds(), /*isForward=*/ true);
    } else {
      this.model_.getCurrentMoveMode().setFocalPoint(
          Math.max(this.model_.getMinY(), this.model_.getFocalPoint()));
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

  private snapToNextLine_(isForward: boolean) {
    if (!this.isEnabled()) {
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
      this.model_.getCurrentMoveMode().initializeSnapIndex(lines, isForward);
      const linesToLog = this.getCurrentLineFocusLines_();
      for (let i = 0; i < linesToLog; i++) {
        chrome.readingMode.incrementLineFocusKeyboardLines();
      }
      return;
    }

    this.updateSnapIndex_(lines, currentIndex, isForward);
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

    const clampedIndex =
        this.model_.getCurrentStyleMode().clampLineIndex(nextIndex);
    this.model_.setCurrentLineIndex(clampedIndex);

    // Calculate visibility bounds to see if we need to scroll.
    const {topRect, bottomRect} =
        this.model_.getCurrentStyleMode().getFocusWindowBounds(
            lines, nextIndex);

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
      this.model_.getCurrentMoveMode().scrollToCenter(lines, clampedIndex);
    } else if (this.model_.getCurrentLineIndex() !== currentIndex) {
      chrome.readingMode.incrementLineFocusKeyboardLines();
      this.model_.getCurrentMoveMode().moveToRect(lines[nextIndex]!);
    }

    // If the user has navigated back to the top of the panel, but there's
    // still a little bit left to scroll, scroll to the top.
    if (this.model_.getCurrentLineIndex() === currentIndex) {
      this.listeners_.forEach(l => l.onNeedScrollToTop());
    }
  }

  private getCurrentLineFocusLines_(): number {
    return this.getCurrentLineFocusStyle().lines;
  }

  private calculateNewPositions_(container: HTMLElement, height: number) {
    this.model_.getCurrentMoveMode().updatePositions(container, height);
    // Adjust line focus to remain at the same text line even if it's moved,
    // due to font or other spacing changes.
    const bounds = this.model_.getTextBounds();
    const newLines = bounds.map(rect => rect.bottom);
    const currentLineIndex = this.model_.getCurrentLineIndex();
    if (!this.isStatic() && currentLineIndex !== null &&
        currentLineIndex >= 0 && currentLineIndex < newLines.length) {
      this.model_.getCurrentMoveMode().setFocalPoint(
          newLines[currentLineIndex]!);
    }
  }

  private setCenterY_() {
    this.model_.getCurrentMoveMode().setFocalPoint(this.model_.getMaxY() / 2);
  }

  // MoveModeDelegate methods.
  notifyMove(): void {
    this.listeners_.forEach(l => l.onLineFocusMove());
  }

  notifyScroll(scrollDiff: number, instant?: boolean): void {
    this.listeners_.forEach(
        l => l.onNeedScrollForLineFocus(scrollDiff, instant));
  }

  onSessionEnd(): void {
    this.logger_.logLineFocusSession();
  }

  static getInstance(): LineFocusController {
    return instance || (instance = new LineFocusController());
  }

  static setInstance(obj: LineFocusController) {
    instance = obj;
  }
}

let instance: LineFocusController|null = null;
