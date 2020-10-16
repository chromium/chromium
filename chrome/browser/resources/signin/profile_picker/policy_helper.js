// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import './strings.js';

/** @return {boolean} */
export function isGuestModeEnabled() {
  return loadTimeData.getBoolean('isGuestModeEnabled');
}

/** @return {boolean} */
export function isProfileCreationAllowed() {
  return loadTimeData.getBoolean('isProfileCreationAllowed');
}

/** @return {boolean} */
export function isBrowserSigninAllowed() {
  return loadTimeData.getBoolean('isBrowserSigninAllowed');
}

/** @return {boolean} */
export function isForceSigninEnabled() {
  const enabled = loadTimeData.getBoolean('isForceSigninEnabled');
  // Force sign in policy is not supported yet. The picker should not be shown
  // in case this policy exists.
  assert(!enabled);
  return enabled;
}

/** @return {boolean} */
export function isSignInProfileCreationSupported() {
  return loadTimeData.getBoolean('signInProfileCreationFlowSupported');
}
