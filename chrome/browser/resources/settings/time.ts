// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const WINDOWS_EPOCH = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
const UNIX_EPOCH = Date.UTC(1970, 0, 1, 0, 0, 0, 0);


/**
 * Converts a JavaScript Date() object to a string that represents microseconds
 * since the Windows FILETIME epoch.
 *
 * The JS Date() is based off of the number of milliseconds since the UNIX epoch
 * (1970-01-01 00::00:00 UTC), while times stored within prefs are represented
 * as the number of microseconds since the Windows FILETIME epoch
 * (1601-01-01 00:00:00 UTC).
 */
export function convertDateToWindowsEpoch(date = Date.now()) {
  const epochDeltaMs = UNIX_EPOCH - WINDOWS_EPOCH;
  return `${(date + epochDeltaMs) * 1000}`;
}
