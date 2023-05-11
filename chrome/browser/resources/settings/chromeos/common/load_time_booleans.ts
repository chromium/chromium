// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Getters for loadTimeData booleans used throughout CrOS Settings.
 * Export them as functions so they reload the values when overridden in tests.
 */
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

export function isCrostiniSupported(): boolean {
  return loadTimeData.getBoolean('isCrostiniSupported');
}
