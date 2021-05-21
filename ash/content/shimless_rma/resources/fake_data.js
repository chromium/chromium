// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State, RmaState, RmadErrorCode} from './shimless_rma_types.js';

/** @type {!Array<!State>} */
export const fakeStates = [
  {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
  {state: RmaState.kSelectComponents, error: RmadErrorCode.kRequestInvalid},
  {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
];

/** @type {!Array<string>} */
export const fakeChromeVersion = [
  '89.0.1232.1',
  '92.0.999.0',
  '95.0.4444.123',
];