// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_REVIEWING_PAGE_STATUS_UI_CONTROLLER_H_
#define CHROME_BROWSER_DICTATION_REVIEWING_PAGE_STATUS_UI_CONTROLLER_H_

#include "base/time/time.h"

namespace tabs {
class TabInterface;
}

namespace dictation {

// Determines whether the "Reviewing page" status UI should be displayed
// during a dictation session.
// - Scoped to each main document (content::Page) independently via
// PageUserData.
// - Expires after 60 minutes so the disclaimer is shown again if the user
//   leaves the same page open.
class ReviewingPageStatusUiController {
 public:
  static constexpr base::TimeDelta kCooldownPeriod = base::Minutes(60);

  ReviewingPageStatusUiController();
  ReviewingPageStatusUiController(const ReviewingPageStatusUiController&) =
      delete;
  ReviewingPageStatusUiController& operator=(
      const ReviewingPageStatusUiController&) = delete;
  ~ReviewingPageStatusUiController();

  // Returns true if the reviewing page status UI should be shown for `tab`,
  // and consumes the show state (starting the 60-minute cooldown for the
  // document).
  bool ConsumeShowReviewingPageStatus(tabs::TabInterface* tab) const;
  void SetEnabledForTesting(bool enabled);

 private:
  bool enabled_for_testing_ = true;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_REVIEWING_PAGE_STATUS_UI_CONTROLLER_H_
