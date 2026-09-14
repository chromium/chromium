// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/keyed_service_provider/feature_engagement_tracker_provider_impl.h"

#include "base/check.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/browser_context.h"

namespace ash {

FeatureEngagementTrackerProviderImpl::FeatureEngagementTrackerProviderImpl() =
    default;

FeatureEngagementTrackerProviderImpl::~FeatureEngagementTrackerProviderImpl() =
    default;

feature_engagement::Tracker* FeatureEngagementTrackerProviderImpl::Find(
    const AccountId& account_id) {
  content::BrowserContext* context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id);
  // Callers resolve a live profile for `account_id` before calling Find(), so a
  // browser context always exists here.
  CHECK(context);
  return feature_engagement::TrackerFactory::GetForBrowserContext(context);
}

}  // namespace ash
