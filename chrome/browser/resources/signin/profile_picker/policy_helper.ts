// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

export function isProfileCreationAllowed(): boolean {
  return loadTimeData.getBoolean('isProfileCreationAllowed');
}

export function isBrowserSigninAllowed(): boolean {
  return loadTimeData.getBoolean('isBrowserSigninAllowed');
}

export function isForceSigninEnabled(): boolean {
  return loadTimeData.getBoolean('isForceSigninEnabled');
}

export function isSignInProfileCreationSupported(): boolean {
  return loadTimeData.getBoolean('signInProfileCreationFlowSupported');
}

export function isAskOnStartupAllowed(): boolean {
  return loadTimeData.getBoolean('isAskOnStartupAllowed');
}
