// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from './common/async_util.js';
import {Flags} from './common/flags.js';
import {InstanceChecker} from './common/instance_checker.js';
import {SACommands} from './switch_access/commands.js';
import {Navigator} from './switch_access/navigator.js';
import {SettingsManager} from './switch_access/settings_manager.js';
import {SwitchAccess} from './switch_access/switch_access.js';

InstanceChecker.closeExtraInstances();

async function initAll() {
  await Flags.init();
  const desktop = await AsyncUtil.getDesktop();
  await SwitchAccess.init(desktop);

  // Navigator must be initialized before other classes.
  Navigator.initializeSingletonInstances(desktop);

  SACommands.init();
  SettingsManager.init();

  SwitchAccess.start();
}

initAll();
