// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_ai_mode_promo_tab_helper.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

namespace {

bool IsNavigationOriginationFromAiSearch(
    content::NavigationHandle& navigation_handle,
    ContextualTasksUiService& ui_service) {
  if (!navigation_handle.GetInitiatorFrameToken().has_value()) {
    return false;
  }
  content::RenderFrameHost* initiator_rfh =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              navigation_handle.GetInitiatorProcessId(),
              navigation_handle.GetInitiatorFrameToken().value()));
  if (!initiator_rfh) {
    return false;
  }
  content::WebContents* source_contents =
      content::WebContents::FromRenderFrameHost(initiator_rfh);
  if (source_contents &&
      ui_service.IsAiUrl(source_contents->GetLastCommittedURL())) {
    return true;
  }
  return false;
}

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchAiModePromoTabHelper);

SearchAiModePromoTabHelper::SearchAiModePromoTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchAiModePromoTabHelper>(*web_contents) {}

SearchAiModePromoTabHelper::~SearchAiModePromoTabHelper() = default;

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

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile);
  // If the user is already signed in, no promo is needed.
  if (!ui_service || ui_service->IsSignedInToBrowserWithValidCredentials()) {
    return;
  }

  // Determine if the navigation was initiated from an AI page.
  bool initiated_from_ai_page =
      IsNavigationOriginationFromAiSearch(*navigation_handle, *ui_service);
  if (!initiated_from_ai_page) {
    return;
  }

  signin_promo_controller_ =
      std::make_unique<SearchAIModeSignInPromoController>(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (tab && tab->GetBrowserWindowInterface()) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(tab->GetBrowserWindowInterface());
    if (browser_view) {
      signin_promo_controller_->ShowPromo(browser_view);
    }
  }
}

}  // namespace contextual_tasks
