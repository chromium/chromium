// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/search_ai_mode_signin_promo_controller_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SearchAIModeSignInPromoController;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace contextual_tasks {

class ContextualTaskNavigationObserver;

// Defines the post-sing in behaviour for the opened search result:
// When enabled, the tab linked to this helper loads the original AIM search
// in the present tab before triggering the side-view of the search result.
// When disabled, the tab load an empty AIM mode search, before proceeding to
// the side view search result.
BASE_DECLARE_FEATURE(kEnableLoadOriginalAIMSearchAfterSigninPromo);

// A tab helper that observes the initial navigation of a tab and shows a
// sign-in promo if the navigation was initiated from an AI page and the user
// is not signed in.
class SearchAiModePromoTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchAiModePromoTabHelper>,
      public signin::IdentityManager::Observer,
      public SearchAIModeSignInPromoControllerObserver {
 public:
  SearchAiModePromoTabHelper(const SearchAiModePromoTabHelper&) = delete;
  SearchAiModePromoTabHelper& operator=(const SearchAiModePromoTabHelper&) =
      delete;
  ~SearchAiModePromoTabHelper() override;

  void FireTimeoutReachedForTesting();
  void SetSigninPromoControllerFactoryForTesting(
      base::RepeatingCallback<
          std::unique_ptr<SearchAIModeSignInPromoController>(
              content::WebContents*)> factory_callback);
  SearchAIModeSignInPromoController* GetSigninPromoControllerForTesting();

 private:
  friend class content::WebContentsUserData<SearchAiModePromoTabHelper>;
  explicit SearchAiModePromoTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

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

  // SearchAIModeSignInPromoControllerObserver implementation:
  void OnFlowAborted() override;

  void TriggerCoBrowsePostSignIn();
  void MaybeTriggerCobrowse(const CoreAccountInfo& account_info);

  bool IsAIModeSearch(content::WebContents* web_contents);
  void MaybeShowPromo();
  // Stops all the observations and destructs `this` object.
  void SelfDestruct();

  raw_ptr<ContextualTasksUiService> contextual_tasks_ui_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  bool has_checked_initial_navigation_ = false;
  bool should_show_promo_ = false;
  bool has_triggered_cobrowse_flow_ = false;
  base::OneShotTimer promo_timer_;
  content::GlobalRenderFrameHostId primary_main_frame_id_;
  base::WeakPtr<content::WebContents> aim_search_web_contents_;
  std::unique_ptr<SearchAIModeSignInPromoController> signin_promo_controller_;
  base::RepeatingCallback<std::unique_ptr<SearchAIModeSignInPromoController>(
      content::WebContents*)>
      signin_promo_controller_factory_for_testing_;
  GURL target_url_;
  std::unique_ptr<ContextualTaskNavigationObserver> contextual_task_observer_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_scoped_observation_{this};
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoControllerObserver>
      signin_promo_controller_observation_{this};
  base::WeakPtrFactory<SearchAiModePromoTabHelper> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_AI_MODE_PROMO_TAB_HELPER_H_
