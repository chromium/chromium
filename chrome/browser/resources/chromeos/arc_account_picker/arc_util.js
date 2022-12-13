// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

/**
 * Options for account addition.
 * @typedef {{
 *   isAvailableInArc: boolean,
 *   showArcAvailabilityPicker: boolean,
 * }}
 */
export let AccountAdditionOptions;

/**
 * @param {?string} json
 * @return {?AccountAdditionOptions}
 */
export function getAccountAdditionOptionsFromJSON(json) {
  if (!json) {
    return null;
  }

  const args = /** @type {AccountAdditionOptions} */ (JSON.parse(json));
  if (!args) {
    return null;
  }

  assert(args.isAvailableInArc !== undefined);
  assert(args.showArcAvailabilityPicker !== undefined);
  return args;
}
