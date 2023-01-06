// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SAVER_DATA_SAVER_H_
#define CHROME_BROWSER_DATA_SAVER_DATA_SAVER_H_

namespace data_saver {

// Overrides the data saver setting when testing.
void OverrideIsDataSaverEnabledForTesting(bool flag);

// Resets the override flag.
void ResetIsDataSaverEnabledForTesting();

// Fetch and cache the Android Data Saver saver setting.
void FetchDataSaverOSSettingAsynchronously();

// Returns true if the Android Data Saver option is enabled and the device is on
// a metered network. On non-Android OSes, this always return false. Note that
// the result returned by this function may be stale. Making OS calls to get the
// state of the Data Saver setting can be slow. For this reason, we make the OS
// calls happen in a background thread and store the result in a global
// variable. Calling IsDataSaverEnabled immediately returns the last cached
// value and fires of OS calls in a background thread. If there is no cached
// value, this function may lookup the setting synchronously, depending on the
// state of the DataSaverSettingBlockWhenUninitialized Finch feature.
bool IsDataSaverEnabled();

}  // namespace data_saver

#endif  // CHROME_BROWSER_DATA_SAVER_DATA_SAVER_H_
