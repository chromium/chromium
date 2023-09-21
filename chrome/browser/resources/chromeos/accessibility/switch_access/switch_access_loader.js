// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '../common/async_util.js';
import {Flags} from '../common/flags.js';
import {InstanceChecker} from '../common/instance_checker.js';

import {SACommands} from './commands.js';
import {Navigator} from './navigator.js';
import {PreferenceManager} from './preference_manager.js';
import {SwitchAccess} from './switch_access.js';

InstanceChecker.closeExtraInstances();

async function initAll() {
  await Flags.init();
  const desktop = await AsyncUtil.getDesktop();
  await SwitchAccess.init(desktop);

  // Navigator must be initialized before other classes.
  Navigator.initializeSingletonInstances(desktop);

  SACommands.init();
  PreferenceManager.initialize();

  SwitchAccess.start();
}

initAll();
