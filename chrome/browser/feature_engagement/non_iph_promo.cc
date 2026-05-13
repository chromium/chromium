// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/non_iph_promo.h"

#include "base/types/pass_key.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

// static
bool NonIphPromo::WouldBeGrantedPermissionToShow(
    content::BrowserContext* context,
    const base::Feature& feature) {
  const Tracker* const tracker = TrackerFactory::GetForBrowserContext(context);
  return tracker->WouldTriggerHelpUI(feature, base::PassKey<NonIphPromo>());
}

// static
bool NonIphPromo::RequestPermissionToShow(content::BrowserContext* context,
                                          const base::Feature& feature) {
  Tracker* const tracker = TrackerFactory::GetForBrowserContext(context);
  if (!tracker->ShouldTriggerHelpUI(feature, base::PassKey<NonIphPromo>())) {
    return false;
  }
  // Immediately dismiss to prevent from blocking other promos.
  tracker->Dismissed(feature, base::PassKey<NonIphPromo>());
  return true;
}

}  // namespace feature_engagement
