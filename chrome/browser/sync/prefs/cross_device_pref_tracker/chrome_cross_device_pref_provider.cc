// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/prefs/cross_device_pref_tracker/chrome_cross_device_pref_provider.h"

#include <string_view>

#include "base/containers/flat_set.h"

ChromeCrossDevicePrefProvider::ChromeCrossDevicePrefProvider() = default;

ChromeCrossDevicePrefProvider::~ChromeCrossDevicePrefProvider() = default;

const base::flat_set<std::string_view>&
ChromeCrossDevicePrefProvider::GetProfilePrefs() const {
  // If needing to add any Chrome-specific prefs, a flat_set can be created here
  // containing the union of common and Chrome-specific prefs and wrapped in
  // base::NoDestructor.

  return common_cross_device_pref_provider_.GetProfilePrefs();
}

const base::flat_set<std::string_view>&
ChromeCrossDevicePrefProvider::GetLocalStatePrefs() const {
  return common_cross_device_pref_provider_.GetLocalStatePrefs();
}
