// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../event_generator.js';

import {CheckContextResult, Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

import ScreenPoint = chrome.accessibilityPrivate.ScreenPoint;
import SyntheticMouseEventButton = chrome.accessibilityPrivate.SyntheticMouseEventButton;

/**
 * Class that implements a macro to send a synthetic single mouse click.
 */
export class MouseClickMacro extends Macro {
  private leftClick_: boolean;
  private location_: ScreenPoint|undefined;

  /**
   * Pass the location in density-independent pixels. Defaults to left click.
   */
  constructor(location: ScreenPoint|undefined, leftClick = true) {
    super(leftClick ? MacroName.MOUSE_CLICK_LEFT : MacroName.MOUSE_CLICK_RIGHT);
    this.leftClick_ = leftClick;
    this.location_ = location;
  }

  /** Invalid context if location isn't set. */
  override checkContext(): CheckContextResult {
    if (!this.location_) {
      return this.createFailureCheckContextResult_(MacroError.BAD_CONTEXT);
    }

    return this.createSuccessCheckContextResult_();
  }

  updateLocation(location?: ScreenPoint): void {
    this.location_ = location;
  }

  override run(): RunMacroResult {
    if (!this.location_) {
      return this.createRunMacroResult_(/*isSuccess=*/ false);
    }
    const mouseButton = this.leftClick_ ? SyntheticMouseEventButton.LEFT :
                                          SyntheticMouseEventButton.RIGHT;

    EventGenerator.sendMousePress(
        this.location_.x, this.location_.y, mouseButton);
    EventGenerator.sendMouseRelease(this.location_.x, this.location_.y);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}

export enum LongClickState {
  PRESS = 0,
  DRAG = 1,
  RELEASE = 2,
  FULFILLED = 3,
}

/**
 * Class that implements a macro to send, hold and release a synthetic mouse
 * click.
 */
export class MouseClickLongMacro extends Macro {
  private location_: ScreenPoint|undefined;
  private state_: LongClickState = LongClickState.PRESS;

  /** Pass the location in density-independent pixels. */
  constructor(location: ScreenPoint|undefined) {
    super(MacroName.MOUSE_LONG_CLICK_LEFT);
    this.location_ = location;
  }

  /**
   * The long click macro should be run at least twice, once when the click
   * begins and again when it ends.
   */
  override triggersAtActionStartAndEnd(): boolean {
    return true;
  }

  /**
   * Invalid context if location isn't set or this macro is already fulfilled.
   */
  override checkContext(): CheckContextResult {
    if (!this.location_) {
      return this.createFailureCheckContextResult_(MacroError.BAD_CONTEXT);
    }

    if (this.state_ === LongClickState.FULFILLED) {
      // This macro cannot be run after has already been fulfilled.
      return this.createFailureCheckContextResult_(
          MacroError.INVALID_USER_INTENT);
    }

    return this.createSuccessCheckContextResult_();
  }

  triggerRelease(): void {
    this.state_ = LongClickState.RELEASE;
  }

  updateLocation(location?: ScreenPoint): void {
    this.location_ = location;
  }

  override run(): RunMacroResult {
    if (!this.location_ || this.state_ === LongClickState.FULFILLED) {
      return this.createRunMacroResult_(/*isSuccess=*/ false);
    }

    switch (this.state_) {
      case LongClickState.PRESS:
        EventGenerator.sendMousePress(
            this.location_.x, this.location_.y, SyntheticMouseEventButton.LEFT);
        this.state_ = LongClickState.DRAG;
        break;
      case LongClickState.DRAG:
        EventGenerator.sendMouseMove(this.location_.x, this.location_.y);
        break;
      case LongClickState.RELEASE:
        EventGenerator.sendMouseRelease(this.location_.x, this.location_.y);
        this.state_ = LongClickState.FULFILLED;
        break;
    }

    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}

/** Class that implements a macro to send a double left click. */
export class MouseClickLeftDoubleMacro extends Macro {
  private location_: ScreenPoint|undefined;

  constructor(location: ScreenPoint|undefined) {
    super(MacroName.MOUSE_CLICK_LEFT_DOUBLE);
    this.location_ = location;
  }

  override checkContext(): CheckContextResult {
    if (!this.location_) {
      return this.createFailureCheckContextResult_(MacroError.BAD_CONTEXT);
    }

    return this.createSuccessCheckContextResult_();
  }

  override run(): RunMacroResult {
    if (!this.location_) {
      return this.createRunMacroResult_(/*isSuccess=*/ false);
    }

    const mouseButton = SyntheticMouseEventButton.LEFT;
    EventGenerator.sendMousePress(
        this.location_.x, this.location_.y, mouseButton,
        /*isDoubleClick=*/ true);
    EventGenerator.sendMouseRelease(
        this.location_.x, this.location_.y, /*isDoubleClick=*/ true);

    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
