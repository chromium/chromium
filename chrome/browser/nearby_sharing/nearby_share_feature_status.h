// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_STATUS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_STATUS_H_

class PrefService;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated. Keep in sync with the NearbyShareError UMA enum defined in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
//
// LINT.IfChange(NearbyShareEnabledState)
enum class NearbyShareEnabledState {
  kEnabledAndOnboarded = 0,
  kEnabledAndNotOnboarded = 1,
  kDisabledAndOnboarded = 2,
  kDisabledAndNotOnboarded = 3,
  kDisallowedByPolicy = 4,
  kMaxValue = kDisallowedByPolicy
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/nearby/enums.xml:NearbyShareEnabledState)

NearbyShareEnabledState GetNearbyShareEnabledState(PrefService* pref_service);

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_STATUS_H_
