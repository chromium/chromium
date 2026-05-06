// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {Segment} from '../read_aloud/read_aloud_types.js';
import {SpeechController} from '../read_aloud/speech_controller.js';
import {isForwardArrow, isLineFocusShortcut, isVerticalArrow} from '../shared/keyboard_util.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {LineFocusModel} from './line_focus_model.js';
import {LineFocusCursorMoveMode, LineFocusNoneMoveMode, LineFocusStaticMoveMode} from './line_focus_move_mode.js';
import type {MoveModeDelegate} from './line_focus_move_mode.js';
import {LineFocusLineStyleMode, LineFocusNoneStyleMode, LineFocusWindowStyleMode} from './line_focus_style_mode.js';
import {getLineFocusValues, LineFocusMovement, LineFocusStyle, LineFocusType} from './read_anything_types.js';

export interface LineFocusListener {
  onLineFocusMove(newTop: number, newHeight: number, newFocalPoint: number):
      void;
  onNeedScrollForLineFocus(scrollDiff: number, instant?: boolean): void;
  onNeedScrollToTop(): void;
  onLineFocusToggled(): void;
  onScrollBufferForLineFocusChange(needsBuffer: boolean): void;
}

// Coordinates the business logic for managing the line focus feature by
// managing the line focus model and delegating movement and style behaviors
// to specialized strategies.
export class LineFocusController implements MoveModeDelegate {
  private readonly listeners_: LineFocusListener[] = [];
  private speechController_ = SpeechController.getInstance();
  private logger_ = ReadAnythingLogger.getInstance();

  constructor(private model_: LineFocusModel = new LineFocusModel()) {
    const styleMode =
        new LineFocusNoneStyleMode(LineFocusStyle.OFF, this.model_);
    this.model_.setCurrentStyleMode(styleMode);
    this.model_.setCurrentMoveMode(new LineFocusNoneMoveMode(
        this.model_, styleMode, this, LineFocusMovement.STATIC));
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

  addListener(listener: LineFocusListener) {
    this.listeners_.push(listener);
  }

  isEnabled(): boolean {
    return chrome.readingMode.isLineFocusEnabled &&
        this.model_.isSessionActive();
  }

  onKeyDown(e: KeyboardEvent, container: HTMLElement, height: number): boolean {
    if (!chrome.readingMode.isLineFocusEnabled) {
      return false;
    }

    if (isLineFocusShortcut(e)) {
      this.toggle_(container, height);
      return true;
    }

    if (isVerticalArrow(e.key) && !this.speechController_.isSpeechActive()) {
      return this.model_.getCurrentMoveMode().snapToNextLine(
          isForwardArrow(e.key));
    }

    return false;
  }

  onScrollEnd(newScrollTop: number) {
    if (chrome.readingMode.isLineFocusEnabled) {
      this.model_.getCurrentMoveMode().onScrollEnd(newScrollTop);
    }
  }

  onMouseMove(y: number) {
    if (chrome.readingMode.isLineFocusEnabled &&
        !this.speechController_.isSpeechActive()) {
      this.model_.getCurrentMoveMode().onMouseMove(y);
    }
  }

  onMouseMoveInToolbar(y: number) {
    if (chrome.readingMode.isLineFocusEnabled &&
        !this.speechController_.isSpeechActive()) {
      this.model_.getCurrentMoveMode().onMouseMoveInToolbar(y);
    }
  }

  onAllMenusClose() {
    this.notifyMove();
  }

  onWordBoundary(segments: Segment[]) {
    if (chrome.readingMode.isLineFocusEnabled) {
      this.model_.getCurrentMoveMode().onWordBoundary(segments);
    }
  }

  onTextLocationsChange(container: HTMLElement, height: number) {
    if (chrome.readingMode.isLineFocusEnabled) {
      this.model_.getCurrentMoveMode().onTextLocationsChange(container, height);
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
    this.updateStrategies_(style, movement);
    this.propagateLineFocus_(style, movement);
    this.model_.getCurrentMoveMode().onActivated(container, height);
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

  // MoveModeDelegate methods.
  notifyMove(): void {
    this.listeners_.forEach(
        l => l.onLineFocusMove(
            this.model_.getTop(), this.model_.getWindowHeight(),
            this.model_.getFocalPoint()));
  }

  notifyScroll(scrollDiff: number, instant?: boolean): void {
    this.listeners_.forEach(
        l => l.onNeedScrollForLineFocus(scrollDiff, instant));
  }

  notifyScrollToTop(): void {
    this.listeners_.forEach(l => l.onNeedScrollToTop());
  }

  notifyScrollBuffer(needsBuffer: boolean): void {
    this.listeners_.forEach(
        l => l.onScrollBufferForLineFocusChange(needsBuffer));
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
