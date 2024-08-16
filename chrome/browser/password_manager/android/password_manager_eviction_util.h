// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_EVICTION_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_EVICTION_UTIL_H_

#include "components/prefs/pref_service.h"

namespace password_manager_upm_eviction {

// Checks whether the current user is currently evicted from the UPM experiment.
bool IsCurrentUserEvicted(const PrefService* prefs);

}  // namespace password_manager_upm_eviction

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_EVICTION_UTIL_H_
