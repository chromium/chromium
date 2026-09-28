// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/reviewing_page_status_ui_controller.h"

#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents.h"

namespace dictation {

namespace {

// Main document-scoped flag (via PageUserData) tracking whether the
// "Reviewing page" disclaimer has been shown for a specific content::Page and
// when it expires.
class ReviewingPageStatusPageData
    : public content::PageUserData<ReviewingPageStatusPageData> {
 public:
  ~ReviewingPageStatusPageData() override = default;

  bool ConsumeShowReviewingPageStatus(base::TimeDelta cooldown_period) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (has_shown_disclaimer_ && now < expiration_time_) {
      return false;
    }
    has_shown_disclaimer_ = true;
    expiration_time_ = now + cooldown_period;
    return true;
  }

 private:
  explicit ReviewingPageStatusPageData(content::Page& page)
      : PageUserData(page) {}
  friend class content::PageUserData<ReviewingPageStatusPageData>;
  PAGE_USER_DATA_KEY_DECL();

  bool has_shown_disclaimer_ = false;
  base::TimeTicks expiration_time_;
};

PAGE_USER_DATA_KEY_IMPL(ReviewingPageStatusPageData);

}  // namespace

ReviewingPageStatusUiController::ReviewingPageStatusUiController() = default;

ReviewingPageStatusUiController::~ReviewingPageStatusUiController() = default;

bool ReviewingPageStatusUiController::ConsumeShowReviewingPageStatus(
    tabs::TabInterface* tab) const {
  if (!enabled_for_testing_ || !tab) {
    return false;
  }

  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return false;
  }

  auto* page_data = ReviewingPageStatusPageData::GetOrCreateForPage(
      contents->GetPrimaryPage());
  return page_data->ConsumeShowReviewingPageStatus(kCooldownPeriod);
}

void ReviewingPageStatusUiController::SetEnabledForTesting(bool enabled) {
  enabled_for_testing_ = enabled;
}

}  // namespace dictation
