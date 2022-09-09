// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BATTERY_BATTERY_SAVER_H_
#define CHROME_BROWSER_BATTERY_BATTERY_SAVER_H_

namespace battery {

// Overrides the battery saver setting when testing.
void OverrideIsBatterySaverEnabledForTesting(bool is_battery_saver_mode);

// Resets the override flag.
void ResetIsBatterySaverEnabledForTesting();

// Returns true if the Android Battery Saver option is enabled. On non-Android
// OSes, this always return false.
bool IsBatterySaverEnabled();

}  // namespace battery

#endif  // CHROME_BROWSER_BATTERY_BATTERY_SAVER_H_
