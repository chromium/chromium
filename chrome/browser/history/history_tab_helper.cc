// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include <algorithm>
#include <utility>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/frame_navigate_params.h"
#include "third_party/blink/public/mojom/referrer.mojom.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/background_tab_manager.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

#if defined(OS_ANDROID)
using chrome::android::BackgroundTabManager;
#endif

using content::NavigationEntry;
using content::WebContents;

HistoryTabHelper::HistoryTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

HistoryTabHelper::~HistoryTabHelper() {
}

void HistoryTabHelper::UpdateHistoryForNavigation(
    const history::HistoryAddPageArgs& add_page_args) {
  history::HistoryService* hs = GetHistoryService();
  if (hs)
    GetHistoryService()->AddPage(add_page_args);
}

history::HistoryAddPageArgs
HistoryTabHelper::CreateHistoryAddPageArgs(
    const GURL& virtual_url,
    base::Time timestamp,
    int nav_entry_id,
    content::NavigationHandle* navigation_handle) {
  // Clicks on content suggestions on the NTP should not contribute to the
  // Most Visited tiles in the NTP.
  const GURL& referrer_url = navigation_handle->GetReferrer().url;
  const bool content_suggestions_navigation =
      referrer_url == ntp_snippets::GetContentSuggestionsReferrerURL() &&
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  const bool status_code_is_error =
      navigation_handle->GetResponseHeaders() &&
      (navigation_handle->GetResponseHeaders()->response_code() >= 400) &&
      (navigation_handle->GetResponseHeaders()->response_code() < 600);
  // Top-level frame navigations are visible; everything else is hidden.
  // Also hide top-level navigations that result in an error in order to
  // prevent the omnibox from suggesting URLs that have never been navigated
  // to successfully.  (If a top-level navigation to the URL succeeds at some
  // point, the URL will be unhidden and thus eligible to be suggested by the
  // omnibox.)
  const bool hidden =
      !ui::PageTransitionIsMainFrame(navigation_handle->GetPageTransition()) ||
      status_code_is_error;
  history::HistoryAddPageArgs add_page_args(
      navigation_handle->GetURL(), timestamp,
      history::ContextIDForWebContents(web_contents()), nav_entry_id,
      navigation_handle->GetReferrer().url,
      navigation_handle->GetRedirectChain(),
      navigation_handle->GetPageTransition(), hidden, history::SOURCE_BROWSED,
      navigation_handle->DidReplaceEntry(), !content_suggestions_navigation,
      navigation_handle->IsSameDocument()
          ? base::Optional<base::string16>(
                navigation_handle->GetWebContents()->GetTitle())
          : base::nullopt);

  // If this navigation attempted a Preview, remove those URLS from the redirect
  // chain so that they are not seen by the user. See http://crbug.com/914404.
  DCHECK(!add_page_args.redirects.empty());
  base::EraseIf(add_page_args.redirects, [](const GURL& url) {
    return previews::IsLitePageRedirectPreviewURL(url);
  });
  if (ui::PageTransitionIsMainFrame(navigation_handle->GetPageTransition()) &&
      virtual_url != navigation_handle->GetURL()) {
    // Hack on the "virtual" URL so that it will appear in history. For some
    // types of URLs, we will display a magic URL that is different from where
    // the page is actually navigated. We want the user to see in history what
    // they saw in the URL bar, so we add the virtual URL as a redirect.  This
    // only applies to the main frame, as the virtual URL doesn't apply to
    // sub-frames.
    add_page_args.url = virtual_url;
    if (!add_page_args.redirects.empty())
      add_page_args.redirects.back() = virtual_url;
  }
  return add_page_args;
}

void HistoryTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  if (navigation_handle->IsInMainFrame()) {
    is_loading_ = true;
    num_title_changes_ = 0;
  } else if (!navigation_handle->HasSubframeNavigationEntryCommitted()) {
    // Filter out unwanted URLs. We don't add auto-subframe URLs that don't
    // change which NavigationEntry is current. They are a large part of history
    // (think iframes for ads) and we never display them in history UI. We will
    // still add manual subframes, which are ones the user has clicked on to
    // get.
    return;
  }

  // Update history. Note that this needs to happen after the entry is complete,
  // which WillNavigate[Main,Sub]Frame will do before this function is called.
  if (!navigation_handle->ShouldUpdateHistory())
    return;

  // Most of the time, the displayURL matches the loaded URL, but for about:
  // URLs, we use a data: URL as the real value.  We actually want to save the
  // about: URL to the history db and keep the data: URL hidden. This is what
  // the WebContents' URL getter does.
  NavigationEntry* last_committed =
      web_contents()->GetController().GetLastCommittedEntry();
  const history::HistoryAddPageArgs& add_page_args = CreateHistoryAddPageArgs(
      web_contents()->GetLastCommittedURL(), last_committed->GetTimestamp(),
      last_committed->GetUniqueID(), navigation_handle);

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (prerender_manager) {
    prerender::PrerenderContents* prerender_contents =
        prerender_manager->GetPrerenderContents(web_contents());
    if (prerender_contents) {
      prerender_contents->DidNavigate(add_page_args);
      return;
    }
  }

#if defined(OS_ANDROID)
  auto* background_tab_manager = BackgroundTabManager::GetInstance();
  if (background_tab_manager->IsBackgroundTab(web_contents())) {
    // No history insertion is done for now since this is a tab that speculates
    // future navigations. Just caching and returning for now.
    background_tab_manager->CacheHistory(add_page_args);
    return;
  }
#else
  // Don't update history if this web contents isn't associated with a tab.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
#endif

  UpdateHistoryForNavigation(add_page_args);
}

void HistoryTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;

  is_loading_ = false;
  last_load_completion_ = base::TimeTicks::Now();
}

void HistoryTabHelper::TitleWasSet(NavigationEntry* entry) {
  if (!entry)
    return;

  // Protect against pages changing their title too often.
  if (num_title_changes_ >= history::kMaxTitleChanges)
    return;

  // Only store page titles into history if they were set while the page was
  // loading or during a brief span after load is complete. This fixes the case
  // where a page uses a title change to alert a user of a situation but that
  // title change ends up saved in history.
  if (is_loading_ || (base::TimeTicks::Now() - last_load_completion_ <
                      history::GetTitleSettingWindow())) {
    history::HistoryService* hs = GetHistoryService();
    if (hs) {
      hs->SetPageTitle(entry->GetVirtualURL(), entry->GetTitleForDisplay());
      ++num_title_changes_;
    }
  }
}

history::HistoryService* HistoryTabHelper::GetHistoryService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return NULL;

  return HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

void HistoryTabHelper::WebContentsDestroyed() {
  // We update the history for this URL.
  WebContents* tab = web_contents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (hs) {
    NavigationEntry* entry = tab->GetController().GetLastCommittedEntry();
    history::ContextID context_id = history::ContextIDForWebContents(tab);
    if (entry) {
      hs->UpdateWithPageEndTime(context_id, entry->GetUniqueID(),
                                tab->GetLastCommittedURL(), base::Time::Now());
    }
    hs->ClearCachedDataForContextID(context_id);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryTabHelper)
