// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_STATUS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_STATUS_H_

class PrefService;

enum class NearbyShareEnabledState {
  kEnabledAndOnboarded = 0,
  kEnabledAndNotOnboarded = 1,
  kDisabledAndOnboarded = 2,
  kDisabledAndNotOnboarded = 3,
  kDisallowedByPolicy = 4,
  kMaxValue = kDisallowedByPolicy
};

NearbyShareEnabledState GetNearbyShareEnabledState(PrefService* pref_service);

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_STATUS_H_
