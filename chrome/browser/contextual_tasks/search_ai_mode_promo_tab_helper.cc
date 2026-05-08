// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_ai_mode_promo_tab_helper.h"

#include <memory>
#include <optional>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {
namespace {
std::optional<base::WeakPtr<content::WebContents>> GetInitiatorWebContents(
    content::NavigationHandle& navigation_handle) {
  if (!navigation_handle.GetInitiatorFrameToken().has_value()) {
    return std::nullopt;
  }
  content::RenderFrameHost* initiator_rfh =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              navigation_handle.GetInitiatorProcessId(),
              navigation_handle.GetInitiatorFrameToken().value()));
  if (!initiator_rfh) {
    return std::nullopt;
  }
  content::WebContents* source_contents =
      content::WebContents::FromRenderFrameHost(initiator_rfh);
  return source_contents ? std::optional<base::WeakPtr<content::WebContents>>(
                               source_contents->GetWeakPtr())
                         : std::nullopt;
}

bool HasValidContextualTaskUrl(content::WebContents* web_contents) {
  return web_contents &&
         IsContextualTasksUrl(web_contents->GetLastCommittedURL());
}

}  // namespace

// A WebContentsObserver that waits for a navigation to the contextual tasks
// UI to complete.
class ContextualTaskNavigationObserver
    : public content::WebContentsObserver,
      public ContextualTasksUIInterface::Observer {
 public:
  ContextualTaskNavigationObserver(content::WebContents* web_contents,
                                   GURL target_url,
                                   base::OnceClosure on_completion_callback)
      : content::WebContentsObserver(web_contents),
        target_url_(target_url),
        contextual_tasks_ui_service_(
            ContextualTasksUiServiceFactory::GetForBrowserContext(
                Profile::FromBrowserContext(
                    web_contents->GetBrowserContext()))),
        on_completion_callback_(std::move(on_completion_callback)) {
    CHECK(contextual_tasks_ui_service_);
    CHECK(on_completion_callback_);
  }

  ~ContextualTaskNavigationObserver() override = default;

 private:
  // ContextualTasksUIInterface::Observer implementation:
  void OnInitComplete() override { MaybeNavigateToTargetUrl(); }

  // content::WebContentsObserver implementation:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->IsInPrimaryMainFrame() ||
        !HasValidContextualTaskUrl(web_contents())) {
      return;
    }
    MaybeNavigateToTargetUrl();
  }

  void MaybeNavigateToTargetUrl() {
    if (!HasValidContextualTaskUrl(web_contents())) {
      return;
    }
    ContextualTasksUIInterface* web_ui_interface =
        GetWebUiInterface(web_contents());
    if (!web_ui_interface) {
      return;
    }
    if (!web_ui_interface->IsInitComplete()) {
      if (!ui_observation_.IsObserving()) {
        ui_observation_.Observe(web_ui_interface);
      }
      return;
    }

    // The WebUI is ready for the contextual task. Trigger the
    // navigation to the target url.
    base::Uuid task_id = ContextualTasksUiService::GetTaskIdFromUrl(
        web_contents()->GetLastCommittedURL());
    CHECK(task_id.is_valid());
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents());
    BrowserWindowInterface* browser = web_ui_interface->GetBrowser();

    if (contextual_tasks_ui_service_) {
      contextual_tasks_ui_service_->OnThreadLinkClicked(
          target_url_, task_id, tab->GetWeakPtr(), browser->GetWeakPtr());
    }
    NotifyComplete();
  }

  void NotifyComplete() {
    ui_observation_.Reset();
    if (on_completion_callback_) {
      std::move(on_completion_callback_).Run();
    }
  }

  const GURL target_url_;
  raw_ptr<ContextualTasksUiService> contextual_tasks_ui_service_;
  base::OnceClosure on_completion_callback_;
  base::ScopedObservation<ContextualTasksUIInterface,
                          ContextualTasksUIInterface::Observer>
      ui_observation_{this};
  base::WeakPtrFactory<ContextualTaskNavigationObserver> weak_ptr_factory_{
      this};
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchAiModePromoTabHelper);

BASE_FEATURE(kEnableLoadOriginalAIMSearchAfterSigninPromo,
             base::FEATURE_DISABLED_BY_DEFAULT);

SearchAiModePromoTabHelper::SearchAiModePromoTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchAiModePromoTabHelper>(*web_contents),
      contextual_tasks_ui_service_(
          ContextualTasksUiServiceFactory::GetForBrowserContext(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      identity_manager_(IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo));
  CHECK(base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks));
  CHECK(contextual_tasks_ui_service_);
}

SearchAiModePromoTabHelper::~SearchAiModePromoTabHelper() = default;

void SearchAiModePromoTabHelper::FireTimeoutReachedForTesting() {
  CHECK_IS_TEST();
  CHECK(promo_timer_.IsRunning());
  promo_timer_.FireNow();
}

void SearchAiModePromoTabHelper::SetSigninPromoControllerFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<SearchAIModeSignInPromoController>(
        content::WebContents* web_contents)> factory_callback) {
  signin_promo_controller_factory_for_testing_ = std::move(factory_callback);
}

SearchAIModeSignInPromoController*
SearchAiModePromoTabHelper::GetSigninPromoControllerForTesting() {
  CHECK_IS_TEST();
  return signin_promo_controller_.get();
}

void SearchAiModePromoTabHelper::TriggerCoBrowsePostSignIn() {
  if (!aim_search_web_contents_ || aim_search_web_contents_.WasInvalidated() ||
      !IsAIModeSearch(aim_search_web_contents_.get()) ||
      !target_url_.is_valid()) {
    SelfDestruct();
    return;
  }

  bool should_load_original_url = base::FeatureList::IsEnabled(
      kEnableLoadOriginalAIMSearchAfterSigninPromo);
  content::NavigationEntry* original_entry =
      aim_search_web_contents_->GetController().GetLastCommittedEntry();
  if (should_load_original_url && !original_entry) {
    SelfDestruct();
    return;
  }
  aim_search_web_contents_.reset();

  // TODO(crbug.com/494541768): Today using the url from the original entry,
  // the follow up conversations are lost (expected behavior for AIM).
  // If this is updated in the future, ensure that the new tab loads the most
  // up-to-date state that we can obtain from the AIM tab.
  const GURL aim_url =
      should_load_original_url
          ? original_entry->GetURL()
          : contextual_tasks_ui_service_->GetDefaultAiPageUrl();

  content::NavigationController::LoadURLParams params(aim_url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  if (should_load_original_url) {
    params.referrer = content::Referrer(original_entry->GetReferrer().url,
                                        original_entry->GetReferrer().policy);
    params.extra_headers = original_entry->GetExtraHeaders();
  }
  has_triggered_cobrowse_flow_ = true;
  web_contents()->GetController().LoadURLWithParams(params);

  // Wait for the contextual task to be ready, then trigger the navigation to
  // the target. Side-view opening is handled by the contextual ui task service.
  contextual_task_observer_ =
      std::make_unique<ContextualTaskNavigationObserver>(
          web_contents(), target_url_,
          base::BindOnce(&SearchAiModePromoTabHelper::SelfDestruct,
                         weak_ptr_factory_.GetWeakPtr()));
}

void SearchAiModePromoTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only care about the first navigation in this tab.
  if (has_checked_initial_navigation_) {
    // Handle navigations that happen after the initial one that opened this tab.
    if (navigation_handle->IsInPrimaryMainFrame() &&
        navigation_handle->HasCommitted() &&
        !navigation_handle->IsSameDocument() && !has_triggered_cobrowse_flow_) {
      SelfDestruct();
    }
    return;
  }
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      !navigation_handle->GetURL().is_valid()) {
    return;
  }
  has_checked_initial_navigation_ = true;

  // If the user is already signed in, no promo is needed.
  if (!contextual_tasks_ui_service_ ||
      contextual_tasks_ui_service_->IsSignedInToBrowserWithValidCredentials()) {
    SelfDestruct();
    return;
  }

  // Determine if the navigation was initiated from an AI search page.
  std::optional<base::WeakPtr<content::WebContents>> initiator =
      GetInitiatorWebContents(*navigation_handle);
  if (!initiator.has_value() || !IsAIModeSearch(initiator.value().get())) {
    SelfDestruct();
    return;
  }
  aim_search_web_contents_ = initiator.value();
  target_url_ = navigation_handle->GetURL();
  primary_main_frame_id_ =
      navigation_handle->GetRenderFrameHost()->GetGlobalId();
  should_show_promo_ = true;
  // Fallback timer to show the promo if the page takes too long to load.
  promo_timer_.Start(FROM_HERE, switches::kSearchAIModePromoPageLoadDelay.Get(),
                     base::BindOnce(&SearchAiModePromoTabHelper::MaybeShowPromo,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void SearchAiModePromoTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  MaybeShowPromo();
}

void SearchAiModePromoTabHelper::MaybeShowPromo() {
  if (!should_show_promo_) {
    return;
  }
  should_show_promo_ = false;
  promo_timer_.Stop();

  // Ensure that we are still looking at the same document that committed.
  if (web_contents()->GetPrimaryMainFrame()->GetGlobalId() !=
      primary_main_frame_id_) {
    SelfDestruct();
    return;
  }

  if (signin_promo_controller_factory_for_testing_) {
    signin_promo_controller_ =
        signin_promo_controller_factory_for_testing_.Run(web_contents());
  } else {
    signin_promo_controller_ =
        std::make_unique<SearchAIModeSignInPromoController>(web_contents());
  }
  signin_promo_controller_observation_.Observe(signin_promo_controller_.get());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (tab && tab->GetBrowserWindowInterface()) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(tab->GetBrowserWindowInterface());
    if (browser_view && identity_manager_) {
      bool promo_triggered =
          signin_promo_controller_->MaybeShowPromo(browser_view);
      if (promo_triggered) {
        identity_manager_scoped_observation_.Observe(identity_manager_);
      }
      // If the promo is not triggered then `this` object is already destructed
      // as `OnFlowAborted` is invoked.
    }
  }
}

void SearchAiModePromoTabHelper::OnFlowAborted() {
  SelfDestruct();
}

void SearchAiModePromoTabHelper::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  auto event_type =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin);
  switch (event_type) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      SelfDestruct();
      return;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      break;
  }
  if (event_details.GetSetPrimaryAccountAccessPoint() !=
      signin_metrics::AccessPoint::kSearchAIModeBubble) {
    // If the user signed in from a different access point then remove this
    // observer.
    if (web_contents()) {
      SelfDestruct();
    }
    return;
  }
  const CoreAccountInfo& account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  MaybeTriggerCobrowse(account_info);
}

void SearchAiModePromoTabHelper::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  MaybeTriggerCobrowse(account_info);
}

void SearchAiModePromoTabHelper::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  MaybeTriggerCobrowse(account_info);
}

void SearchAiModePromoTabHelper::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  SelfDestruct();
}

void SearchAiModePromoTabHelper::MaybeTriggerCobrowse(
    const CoreAccountInfo& account_info) {
  if (!identity_manager_scoped_observation_.IsObserving() ||
      !contextual_tasks_ui_service_) {
    return;
  }
  if (!contextual_tasks_ui_service_
           ->IsSignedInToBrowserWithValidCredentials()) {
    return;
  }
  if (identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
      account_info.account_id) {
    return;
  }
  identity_manager_scoped_observation_.Reset();
  TriggerCoBrowsePostSignIn();
}

bool SearchAiModePromoTabHelper::IsAIModeSearch(
    content::WebContents* web_contents) {
  if (!web_contents || !contextual_tasks_ui_service_) {
    return false;
  }
  return contextual_tasks_ui_service_->IsAiUrl(
      web_contents->GetLastCommittedURL());
}

void SearchAiModePromoTabHelper::SelfDestruct() {
  contextual_task_observer_.reset();
  identity_manager_scoped_observation_.Reset();
  signin_promo_controller_observation_.Reset();
  if (web_contents()) {
    web_contents()->RemoveUserData(SearchAiModePromoTabHelper::UserDataKey());
  }
}
}  // namespace contextual_tasks
