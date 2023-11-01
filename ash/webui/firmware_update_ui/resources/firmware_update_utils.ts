// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

// Return whether v2 of the Firmware Updates app is enabled.
export const isAppV2Enabled = (): boolean => {
  return loadTimeData.getBoolean('isFirmwareUpdateUIV2Enabled');
};
