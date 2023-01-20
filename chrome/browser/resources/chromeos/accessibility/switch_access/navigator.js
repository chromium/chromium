// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ItemScanManager} from './item_scan_manager.js';
import {ItemNavigatorInterface, PointNavigatorInterface} from './navigator_interfaces.js';
import {PointScanManager} from './point_scan_manager.js';
import {SwitchAccess} from './switch_access.js';
import {ErrorType} from './switch_access_constants.js';

const AutomationNode = chrome.automation.AutomationNode;

export class Navigator {
  /** @param {!AutomationNode} desktop */
  static initializeSingletonInstances(desktop) {
    Navigator.itemManager_ = new ItemScanManager(desktop);
    Navigator.pointManager_ = new PointScanManager();
  }

  /** @return {!ItemNavigatorInterface} */
  static get byItem() {
    if (!Navigator.itemManager_) {
      throw SwitchAccess.error(
          ErrorType.UNINITIALIZED,
          'Cannot access itemManager before Navigator.init()');
    }
    return Navigator.itemManager_;
  }

  /** @return {!PointNavigatorInterface} */
  static get byPoint() {
    if (!Navigator.pointManager_) {
      throw SwitchAccess.error(
          ErrorType.UNINITIALIZED,
          'Cannot access pointManager before Navigator.init()');
    }
    return Navigator.pointManager_;
  }
}

/** @private {ItemNavigatorInterface} */
Navigator.itemManager_;

/** @private {PointNavigatorInterface} */
Navigator.pointManager_;
