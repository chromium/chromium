// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../event_generator.js';

import {CheckContextResult, Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

import ScreenPoint = chrome.accessibilityPrivate.ScreenPoint;
import SyntheticMouseEventButton = chrome.accessibilityPrivate.SyntheticMouseEventButton;

/**
 * Class that implements a macro to send a single synthetic mouse click.
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
