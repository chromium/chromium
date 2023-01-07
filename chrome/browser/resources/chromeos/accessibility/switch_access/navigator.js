// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ItemScanManager} from './item_scan_manager.js';
import {ItemNavigatorInterface, PointNavigatorInterface} from './navigator_interface.js';
import {PointScanManager} from './point_scan_manager.js';

export class Navigator {
  /** @param {!chrome.automation.AutomationNode} desktop */
  static initializeSingletonInstance(desktop) {
    Navigator.item_manager_ = new ItemScanManager(desktop);
    Navigator.point_manager_ = new PointScanManager();
    globalThis.getTreeForDebugging =
        Navigator.item_manager_.getTreeForDebugging.bind(
            Navigator.item_manager_);
  }

  /** @return {!ItemNavigatorInterface} */
  static get byItem() {
    return Navigator.item_manager_;
  }

  /** @return {!PointNavigatorInterface} */
  static get byPoint() {
    return Navigator.point_manager_;
  }
}
