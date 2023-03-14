// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InstanceChecker} from '../common/instance_checker.js';

import {SelectToSpeak} from './select_to_speak.js';

export let selectToSpeak: SelectToSpeak;

if (InstanceChecker.isActiveInstance()) {
  selectToSpeak = new SelectToSpeak();
}
