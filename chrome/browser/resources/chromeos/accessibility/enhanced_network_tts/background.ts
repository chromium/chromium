// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/common/testing/test_import_manager.js';

import {InstanceChecker} from '/common/instance_checker.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {EnhancedNetworkTts} from './enhanced_network_tts.js';

InstanceChecker.closeExtraInstances();
export const enhancedNetworkTts = new EnhancedNetworkTts();
TestImportManager.exportForTesting(['enhancedNetworkTts', enhancedNetworkTts]);
