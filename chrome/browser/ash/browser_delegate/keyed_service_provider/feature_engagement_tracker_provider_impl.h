// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_FEATURE_ENGAGEMENT_TRACKER_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_FEATURE_ENGAGEMENT_TRACKER_PROVIDER_IMPL_H_

#include "chromeos/ash/components/feature_engagement/feature_engagement_tracker_provider.h"

class AccountId;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace ash {

// //chrome-side implementation of FeatureEngagementTrackerProvider. Wraps
// //chrome/browser/feature_engagement's Profile-keyed TrackerFactory so
// ChromeOS callers can reach the service through the chromeos-side interface.
class FeatureEngagementTrackerProviderImpl
    : public FeatureEngagementTrackerProvider {
 public:
  FeatureEngagementTrackerProviderImpl();
  FeatureEngagementTrackerProviderImpl(
      const FeatureEngagementTrackerProviderImpl&) = delete;
  FeatureEngagementTrackerProviderImpl& operator=(
      const FeatureEngagementTrackerProviderImpl&) = delete;
  ~FeatureEngagementTrackerProviderImpl() override;

  // FeatureEngagementTrackerProvider:
  feature_engagement::Tracker* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_FEATURE_ENGAGEMENT_TRACKER_PROVIDER_IMPL_H_
