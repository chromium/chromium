// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_NON_IPH_PROMO_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_NON_IPH_PROMO_H_

#include "base/feature.h"

namespace content {
class BrowserContext;
}

namespace feature_engagement {

// Provides a utility to let systems rate-limit their promotional UI using
// `feature_engagement::Tracker` without engaging with the Desktop IPH system
// in a way that does not interact or interfere with other promos.
//
// Since the Feature Engagement tracker only allows one active feature at a time
// (and since each feature must be explicitly dismissed to prevent the feature
// from remaining active forever), and since IPHs use Feature Engagement but are
// managed by a separate system, this utility is provided to avoid non-IPH
// promos from colliding with or blocking IPH and other systems.
//
// Usage notes:
//
// If you use this system, you should configure your Feature Engagement feature
// via `//components/feature_engagement/public/feature_configurations.cc` - this
// is in contrast with IPH, which should not use that file to configure promos.
//
// Be sure to use session configuration so that your promo only blocks features
// which it should actually block (e.g. if you are managing a family of non-IPH
// promos it would make sense to have them all block each other in the same
// session) - or just in general not allow more than one in a specific period.
// See `//components/feature_engagement/README.md` for more information on how
// to properly configure promos.
class NonIphPromo {
 public:
  // Returns true if `feature` would be allowed to show in `context` if
  // requested right now.  Does not mark the feature as shown in the Feature
  // Engagement Tracker.
  [[nodiscard]] static bool WouldBeGrantedPermissionToShow(
      content::BrowserContext* context,
      const base::Feature& feature);

  // Asks to be allowed to show `feature` in `context`, and returns the result,
  // based on the feature's Feature Engagement Tracker configuration. If true,
  // the caller should show the UI, and the feature will be marked as "shown" in
  // the Feature Engagement Tracker.
  //
  // On success, this also immediately marks the feature as "Dismissed" in the
  // Feature Engagement Tracker as well, to avoid blocking other, unrelated
  // promos.
  //
  // If you are not yet certain if you want to show the UI and don't want to
  // prematurely mark your promo as "shown" (thus possibly preventing it from
  // actually being shown in the future), you can first call
  // `WouldBeGrantedPermissionToShow()` - just remember to call this function
  // when you're sure you want to go ahead with showing the UI.
  [[nodiscard]] static bool RequestPermissionToShow(
      content::BrowserContext* context,
      const base::Feature& feature);

 private:
  NonIphPromo() = default;
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_NON_IPH_PROMO_H_
