// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Needed for testing.
import '/common/async_util.js';
import '/common/event_generator.js';

import {KeepAlive} from '/common/keep_alive.js';
import {InstanceChecker} from '/common/mv3/instance_checker.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {SelectToSpeak} from './select_to_speak.js';

export let selectToSpeak: SelectToSpeak;

if (InstanceChecker.isActiveInstance()) {
  // Prevent this service worker from going to sleep.
  KeepAlive.init();

  selectToSpeak = new SelectToSpeak();
  TestImportManager.exportForTesting(['selectToSpeak', selectToSpeak]);
}
