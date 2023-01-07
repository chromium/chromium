// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_SHARED_PREFERENCES_MIGRATOR_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_SHARED_PREFERENCES_MIGRATOR_ANDROID_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace android::shared_preferences {

// Attempts to get the value of a `SharedPreference` and then remove it.
// Returns `absl::nullopt` if the key cannot be found.
absl::optional<bool> GetAndClearBoolean(
    const std::string& shared_preference_key);

}  // namespace android::shared_preferences

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_SHARED_PREFERENCES_MIGRATOR_ANDROID_H_
