// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineResult, RoutineType} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

/**
 * @fileoverview
 * This file contains shared types for the network diagnostics components.
 */

/**
 * A routine response from the Network Diagnostics mojo service.
 * @typedef {{
 *   result: RoutineResult,
 * }}
 */
export let RoutineResponse;

/**
 * A network diagnostics routine. Holds descriptive information about the
 * routine, and it's transient state.
 * @typedef {{
 *   name: string,
 *   type: !RoutineType,
 *   group: !RoutineGroup,
 *   func: function(),
 *   running: boolean,
 *   resultMsg: string,
 *   result: ?RoutineResult,
 * }}
 */
export let Routine;

/**
 * Definition for different groups of network routines.
 * @enum {number}
 */
export const RoutineGroup = {
  CONNECTION: 0,
  WIFI: 1,
  PORTAL: 2,
  GATEWAY: 3,
  FIREWALL: 4,
  DNS: 5,
  GOOGLE_SERVICES: 6,
  ARC: 7,
};

export const Icons = {
  TEST_FAILED: 'test_failed.png',
  TEST_NOT_RUN: 'test_not_run.png',
  TEST_PASSED: 'test_passed.png',
};
