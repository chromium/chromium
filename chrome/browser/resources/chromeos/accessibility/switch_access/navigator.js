// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ItemScanManager} from './item_scan_manager.js';
import {ItemNavigatorInterface} from './navigator_interface.js';

export class Navigator {
  /** @param {!chrome.automation.AutomationNode} desktop */
  static initializeSingletonInstance(desktop) {
    Navigator.item_manager_ = new ItemScanManager(desktop);
    window.getTreeForDebugging =
        Navigator.item_manager_.getTreeForDebugging.bind(
            Navigator.item_manager_);
  }

  /** @return {!ItemNavigatorInterface} */
  static get byItem() {
    return Navigator.item_manager_;
  }
}
