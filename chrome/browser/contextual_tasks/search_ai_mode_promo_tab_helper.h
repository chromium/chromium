// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_

#include <memory>

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SearchAIModeSignInPromoController;

namespace contextual_tasks {

// A tab helper that observes the initial navigation of a tab and shows a
// sign-in promo if the navigation was initiated from an AI page and the user
// is not signed in.
class SearchAiModePromoTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchAiModePromoTabHelper>,
      public signin::IdentityManager::Observer,
      public ContextualTasksUiService::Observer {
 public:
  SearchAiModePromoTabHelper(const SearchAiModePromoTabHelper&) = delete;
  SearchAiModePromoTabHelper& operator=(const SearchAiModePromoTabHelper&) =
      delete;
  ~SearchAiModePromoTabHelper() override;

 private:
  friend class content::WebContentsUserData<SearchAiModePromoTabHelper>;
  explicit SearchAiModePromoTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // signin::IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // ContextualTasksUiService::Observer:
  void OnContextualTasksUiServiceShutdown(
      ContextualTasksUiService* service) override;

  void TriggerCoBrowsePostSignIn();
  void MaybeTriggerCobrowse(const CoreAccountInfo& account_info);

  bool IsAIModeSearch(content::WebContents* web_contents);

  raw_ptr<ContextualTasksUiService> contextual_tasks_ui_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  bool has_checked_initial_navigation_ = false;
  base::WeakPtr<content::WebContents> aim_search_web_contents_;
  std::unique_ptr<SearchAIModeSignInPromoController> signin_promo_controller_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_scoped_observation_{this};
  base::ScopedObservation<ContextualTasksUiService,
                          ContextualTasksUiService::Observer>
      contextual_tasks_ui_service_scoped_observation_{this};
  base::WeakPtrFactory<SearchAiModePromoTabHelper> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_
