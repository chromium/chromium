// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PREFS_CROSS_DEVICE_PREF_TRACKER_CHROME_CROSS_DEVICE_PREF_PROVIDER_H_
#define CHROME_BROWSER_SYNC_PREFS_CROSS_DEVICE_PREF_TRACKER_CHROME_CROSS_DEVICE_PREF_PROVIDER_H_

#include <string_view>

#include "base/containers/flat_set.h"
#include "components/sync_preferences/cross_device_pref_tracker/common_cross_device_pref_provider.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_provider.h"

// Chrome implementation of `CrossDevicePrefProvider`.
class ChromeCrossDevicePrefProvider
    : public sync_preferences::CrossDevicePrefProvider {
 public:
  ChromeCrossDevicePrefProvider();
  ~ChromeCrossDevicePrefProvider() override;

  // `CrossDevicePrefProvider` overrides:
  const base::flat_set<std::string_view>& GetProfilePrefs() const override;
  const base::flat_set<std::string_view>& GetLocalStatePrefs() const override;

 private:
  // This defines the list of prefs that are tracked across all platforms.
  sync_preferences::CommonCrossDevicePrefProvider
      common_cross_device_pref_provider_;
  // A list of all profile prefs to be tracked on Chrome.
  const base::flat_set<std::string_view> profile_prefs_;
  // A list of all local-state prefs to be tracked on Chrome.
  const base::flat_set<std::string_view> local_state_prefs_;
};

#endif  // CHROME_BROWSER_SYNC_PREFS_CROSS_DEVICE_PREF_TRACKER_CHROME_CROSS_DEVICE_PREF_PROVIDER_H_
