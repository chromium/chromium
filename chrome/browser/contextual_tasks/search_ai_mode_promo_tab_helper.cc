// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_ai_mode_promo_tab_helper.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
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
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
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

bool HasValidContextualTaskUrl(
    content::WebContents* web_contents,
    ContextualTasksUiService* contextual_tasks_ui_service) {
  GURL contextual_task_url = GURL(chrome::kChromeUIContextualTasksURL);
  return web_contents && contextual_tasks_ui_service->IsContextualTasksUrl(
                             web_contents->GetLastCommittedURL());
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
  }

  ~ContextualTaskNavigationObserver() override = default;

 private:
  // ContextualTasksUIInterface::Observer implementation:
  void OnInitComplete() override { MaybeNavigateToTargetUrl(); }

  // content::WebContentsObserver implementation:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->IsInPrimaryMainFrame() ||
        !HasValidContextualTaskUrl(web_contents(),
                                   contextual_tasks_ui_service_.get())) {
      return;
    }
    MaybeNavigateToTargetUrl();
  }

  void MaybeNavigateToTargetUrl() {
    if (!HasValidContextualTaskUrl(web_contents(),
                                   contextual_tasks_ui_service_.get())) {
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

SearchAiModePromoTabHelper::SearchAiModePromoTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchAiModePromoTabHelper>(*web_contents),
      contextual_tasks_ui_service_(
          ContextualTasksUiServiceFactory::GetForBrowserContext(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      identity_manager_(IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    if (base::FeatureList::IsEnabled(
            switches::kEnableSearchAIModeSigninPromo)) {
      // TODO(crbug.com/486858611): This tab helper will be created only if the
      // feature is enabled.
      CHECK(contextual_tasks_ui_service_);
    }
}

SearchAiModePromoTabHelper::~SearchAiModePromoTabHelper() = default;

void SearchAiModePromoTabHelper::TriggerCoBrowsePostSignIn() {
  if (!aim_search_web_contents_ || aim_search_web_contents_.WasInvalidated() ||
      !IsAIModeSearch(aim_search_web_contents_.get())) {
    return;
  }
  content::NavigationEntry* original_entry =
      aim_search_web_contents_->GetController().GetLastCommittedEntry();
  aim_search_web_contents_.reset();

  if (!original_entry || !target_url_.is_valid()) {
    return;
  }
  content::NavigationController::LoadURLParams params(original_entry->GetURL());
  params.referrer = content::Referrer(original_entry->GetReferrer().url,
                                      original_entry->GetReferrer().policy);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  params.extra_headers = original_entry->GetExtraHeaders();
  // Load the AIM experience into the current tab (previously opened url from
  // AIM).
  // TODO(crbug.com/494541768): Today this gives us the original query of the
  // user, the follow up conversations are lost (expected behavior for AIM).
  // If this is updated in the future, ensure that the new tab loads the most
  // up-to-date state that we can obtain from the AIM tab.
  web_contents()->GetController().LoadURLWithParams(params);

  // Wait for the contextual task to be ready, then trigger the navigation to
  // the target. Side-view opening is handled by the contextual ui task service.
  contextual_task_observer_ =
      std::make_unique<ContextualTaskNavigationObserver>(
          web_contents(), target_url_,
          base::BindOnce(
              &SearchAiModePromoTabHelper::OnSearchResultNavigationComplete,
              weak_ptr_factory_.GetWeakPtr()));
}

void SearchAiModePromoTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only care about the first navigation in this tab.
  if (has_checked_initial_navigation_) {
    return;
  }
  if (!base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo)) {
    return;
  }
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }
  has_checked_initial_navigation_ = true;

  // If the user is already signed in, no promo is needed.
  if (!contextual_tasks_ui_service_ ||
      contextual_tasks_ui_service_->IsSignedInToBrowserWithValidCredentials()) {
    return;
  }

  // Determine if the navigation was initiated from an AI page.
  std::optional<base::WeakPtr<content::WebContents>> initiator =
      GetInitiatorWebContents(*navigation_handle);
  if (!initiator.has_value() || !IsAIModeSearch(initiator.value().get())) {
    return;
  }
  aim_search_web_contents_ = initiator.value();
  target_url_ = navigation_handle->GetURL();
  signin_promo_controller_ =
      std::make_unique<SearchAIModeSignInPromoController>(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (tab && tab->GetBrowserWindowInterface()) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(tab->GetBrowserWindowInterface());
    if (browser_view && identity_manager_) {
      identity_manager_scoped_observation_.Observe(identity_manager_);
      signin_promo_controller_->ShowPromo(browser_view);
    }
  }
}

void SearchAiModePromoTabHelper::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }
  if (event_details.GetSetPrimaryAccountAccessPoint() !=
      signin_metrics::AccessPoint::kSearchAIModeBubble) {
    // We can stop observing early in that case, as the purpose of this class
    // is to promote sign-in to the user.
    identity_manager_scoped_observation_.Reset();
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
  identity_manager_scoped_observation_.Reset();
  identity_manager_ = nullptr;
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

void SearchAiModePromoTabHelper::OnSearchResultNavigationComplete() {
  contextual_task_observer_.reset();
}

}  // namespace contextual_tasks
