// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ItemScanManager} from './item_scan_manager.js';
import {NavigatorInterface} from './navigator_interface.js';

export class Navigator {
  /** @param {!chrome.automation.AutomationNode} desktop */
  static initializeSingletonInstance(desktop) {
    Navigator.instance_ = new ItemScanManager(desktop);
    window.getTreeForDebugging =
        Navigator.instance_.getTreeForDebugging.bind(Navigator.instance_);
  }

  /** @type {!NavigatorInterface} instance */
  static get instance() {
    return Navigator.instance_;
  }
}
