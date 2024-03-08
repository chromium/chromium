// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../assert.js';
import {LaunchType} from '../metrics.js';

import * as mojoType from './type.js';

/**
 * Converts the launch type to the mojo enum to be used in metrics.
 */
export function convertLaunchTypeToMojo(launchType: LaunchType):
    mojoType.LaunchType {
  switch (launchType) {
    case LaunchType.ASSISTANT:
      return mojoType.LaunchType.kAssistant;
    case LaunchType.DEFAULT:
      return mojoType.LaunchType.kDefault;
    default:
      assertNotReached();
  }
}
