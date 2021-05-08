// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const PointScanState = chrome.accessibilityPrivate.PointScanState;

import {ActionManager} from './action_manager.js';
import {FocusRingManager} from './focus_ring_manager.js';
import {PointNavigatorInterface} from './navigator_interface.js';
import {SAConstants} from './switch_access_constants.js';
import {SwitchAccess} from './switch_access.js';

export class PointScanManager extends PointNavigatorInterface {
  constructor() {
    super();
    /** @private {!constants.Point} */
    this.point_ = {x: 0, y: 0};

    /** @private {function(constants.Point)} */
    this.pointListener_ = this.handleOnPointScanSet_.bind(this);
  }

  // ====== PointNavigatorInterface implementation =====

  /** @return {!constants.Point} */
  get currentPoint() {
    return this.point_;
  }

  start() {
    SwitchAccess.mode = SAConstants.Mode.POINT_SCAN;
    FocusRingManager.clearAll();
    chrome.accessibilityPrivate.onPointScanSet.addListener(this.pointListener_);
    chrome.accessibilityPrivate.setPointScanState(PointScanState.START);
  }

  stop() {
    chrome.accessibilityPrivate.setPointScanState(PointScanState.STOP);
    SwitchAccess.mode = SAConstants.Mode.ITEM_SCAN;
  }

  // ============= Private Methods =============

  /**
   * Shows the point scan menu and sets the point scan position
   * coordinates.
   * @param {!constants.Point} point
   * @private
   */
  handleOnPointScanSet_(point) {
    this.point_ = point;
    ActionManager.openMenu(SAConstants.MenuType.POINT_SCAN_MENU);
    chrome.accessibilityPrivate.onPointScanSet.removeListener(
        this.pointListener_);
  }
}
