// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_

#include <memory>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SearchAIModeSignInPromoController;

namespace contextual_tasks {

// A tab helper that observes the initial navigation of a tab and shows a
// sign-in promo if the navigation was initiated from an AI page and the user
// is not signed in.
class SearchAiModePromoTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchAiModePromoTabHelper> {
 public:
  SearchAiModePromoTabHelper(const SearchAiModePromoTabHelper&) = delete;
  SearchAiModePromoTabHelper& operator=(const SearchAiModePromoTabHelper&) =
      delete;
  ~SearchAiModePromoTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<SearchAiModePromoTabHelper>;
  explicit SearchAiModePromoTabHelper(content::WebContents* web_contents);

  bool has_checked_initial_navigation_ = false;
  std::unique_ptr<SearchAIModeSignInPromoController> signin_promo_controller_;

  base::WeakPtrFactory<SearchAiModePromoTabHelper> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_
