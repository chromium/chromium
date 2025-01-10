// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ItemScanManager} from './item_scan_manager.js';
import {ItemNavigatorInterface, PointNavigatorInterface} from './navigator_interfaces.js';
import {PointScanManager} from './point_scan_manager.js';
import {SwitchAccess} from './switch_access.js';
import {ErrorType} from './switch_access_constants.js';

type AutomationNode = chrome.automation.AutomationNode;

export class Navigator {
  private static itemManager_?: ItemScanManager;
  private static pointManager_?: PointScanManager;

  static initializeSingletonInstances(desktop: AutomationNode): void {
    Navigator.itemManager_ = new ItemScanManager(desktop);
    Navigator.pointManager_ = new PointScanManager();
  }

  static get byItem(): ItemNavigatorInterface {
    if (!Navigator.itemManager_) {
      throw SwitchAccess.error(
          ErrorType.UNINITIALIZED,
          'Cannot access itemManager before Navigator.init()');
    }
    return Navigator.itemManager_;
  }

  static get byPoint(): PointNavigatorInterface {
    if (!Navigator.pointManager_) {
      throw SwitchAccess.error(
          ErrorType.UNINITIALIZED,
          'Cannot access pointManager before Navigator.init()');
    }
    return Navigator.pointManager_;
  }
}

TestImportManager.exportForTesting(Navigator);
