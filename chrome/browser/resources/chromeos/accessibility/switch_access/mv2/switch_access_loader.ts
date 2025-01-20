// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/common/testing/test_import_manager.js';

import {AsyncUtil} from '/common/async_util.js';
import {Flags} from '/common/flags.js';
import {InstanceChecker} from '/common/instance_checker.js';

import {ActionManager} from './action_manager.js';
import {AutoScanManager} from './auto_scan_manager.js';
import {SACommands} from './commands.js';
import {FocusRingManager} from './focus_ring_manager.js';
import {Navigator} from './navigator.js';
import {SettingsManager} from './settings_manager.js';
import {SwitchAccess} from './switch_access.js';

InstanceChecker.closeExtraInstances();

async function initAll(): Promise<void> {
  await Flags.init();
  const desktop = await AsyncUtil.getDesktop();
  await SwitchAccess.init(desktop);

  // Navigator must be initialized before other classes.
  Navigator.initializeSingletonInstances(desktop);

  ActionManager.init();
  AutoScanManager.init();
  FocusRingManager.init();
  SACommands.init();
  SettingsManager.init();

  SwitchAccess.start();
}

initAll();
