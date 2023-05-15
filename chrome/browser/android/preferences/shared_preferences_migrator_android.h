// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_SHARED_PREFERENCES_MIGRATOR_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_SHARED_PREFERENCES_MIGRATOR_ANDROID_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace android::shared_preferences {

// Clears the `SharedPreference` value for `shared_preference_key`.
void ClearKey(const std::string& shared_preference_key);

// Attempts to get the value of a `SharedPreference` and then remove it.
// Returns `absl::nullopt` if the key cannot be found.
absl::optional<bool> GetAndClearBoolean(
    const std::string& shared_preference_key);

// Attempts to get the value of a `SharedPreference` and then remove it.
// Returns `absl::nullopt` if the key cannot be found.
absl::optional<int> GetAndClearInt(const std::string& shared_preference_key);

// Attempts to read a value of a `SharedPreference` returning
// `default_value` if the value is empty.
std::string GetString(const std::string& shared_preference_key,
                      const std::string& default_value);

// Attempts to write `value` as a `SharedPreference` value.
void SetString(const std::string& shared_preference_key,
               const std::string& value);

}  // namespace android::shared_preferences

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_SHARED_PREFERENCES_MIGRATOR_ANDROID_H_
