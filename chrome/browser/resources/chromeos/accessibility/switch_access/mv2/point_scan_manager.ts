// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {constants} from '/common/constants.js';
import {EventGenerator, MouseClickParams} from '/common/event_generator.js';

import {ActionManager} from './action_manager.js';
import {FocusRingManager} from './focus_ring_manager.js';
import {PointNavigatorInterface} from './navigator_interfaces.js';
import {SwitchAccess} from './switch_access.js';
import {MenuType, Mode} from './switch_access_constants.js';

import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
import Point = constants.Point;
import PointScanState = chrome.accessibilityPrivate.PointScanState;

export class PointScanManager implements PointNavigatorInterface {
  private point_: Point = {x: 0, y: 0};
  private pointListener_: (point: Point) => void;

  constructor() {
    this.pointListener_ = point => this.handleOnPointScanSet_(point);
  }

  // ====== PointNavigatorInterface implementation =====

  get currentPoint(): Point {
    return this.point_;
  }

  start(): void {
    FocusRingManager.clearAll();
    SwitchAccess.mode = Mode.POINT_SCAN;
    chrome.accessibilityPrivate.onPointScanSet.addListener(this.pointListener_);
    chrome.accessibilityPrivate.setPointScanState(PointScanState.START);
  }

  stop(): void {
    chrome.accessibilityPrivate.setPointScanState(PointScanState.STOP);
  }

  performMouseAction(action: MenuAction): void {
    if (SwitchAccess.mode !== Mode.POINT_SCAN) {
      return;
    }
    if (action !== MenuAction.LEFT_CLICK && action !== MenuAction.RIGHT_CLICK) {
      return;
    }

    const params: MouseClickParams = {};
    if (action === MenuAction.RIGHT_CLICK) {
      params.mouseButton =
          chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT;
    }

    EventGenerator.sendMouseClick(this.point_.x, this.point_.y, params);
    this.start();
  }

  // ============= Private Methods =============

  /**
   * Shows the point scan menu and sets the point scan position
   * coordinates.
   */
  private handleOnPointScanSet_(point: Point): void {
    this.point_ = point;
    ActionManager.openMenu(MenuType.POINT_SCAN_MENU);
    chrome.accessibilityPrivate.onPointScanSet.removeListener(
        this.pointListener_);
  }
}
