// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/background_tab_manager.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

namespace {

using content::NavigationEntry;
using content::WebContents;
#if defined(OS_ANDROID)
using chrome::android::BackgroundTabManager;
#endif

#if defined(OS_ANDROID)
bool IsNavigationFromFeed(content::WebContents& web_contents, const GURL& url) {
  feed::FeedService* feed_service =
      feed::FeedServiceFactory::GetForBrowserContext(
          web_contents.GetBrowserContext());
  if (!feed_service)
    return false;

  return feed_service->GetStream()->WasUrlRecentlyNavigatedFromFeed(url);
}

#endif

bool ShouldConsiderForNtpMostVisited(
    content::WebContents& web_contents,
    content::NavigationHandle* navigation_handle) {
#if defined(OS_ANDROID)
  // Clicks on content suggestions on the NTP should not contribute to the
  // Most Visited tiles in the NTP.
  DCHECK(!navigation_handle->GetRedirectChain().empty());
  if (ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK) &&
      IsNavigationFromFeed(web_contents,
                           navigation_handle->GetRedirectChain()[0])) {
    return false;
  }
#endif

  return true;
}

}  // namespace

HistoryTabHelper::HistoryTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

HistoryTabHelper::~HistoryTabHelper() {}

void HistoryTabHelper::UpdateHistoryForNavigation(
    const history::HistoryAddPageArgs& add_page_args) {
  history::HistoryService* hs = GetHistoryService();
  if (hs)
    hs->AddPage(add_page_args);
}

history::HistoryAddPageArgs HistoryTabHelper::CreateHistoryAddPageArgs(
    const GURL& virtual_url,
    base::Time timestamp,
    int nav_entry_id,
    content::NavigationHandle* navigation_handle) {
  const ui::PageTransition page_transition =
      navigation_handle->GetPageTransition();
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

  // If the full referrer URL is provided, use that. Otherwise, we probably have
  // an incomplete referrer due to referrer policy (empty or origin-only).
  // Fall back to the previous main frame URL if the referrer policy required
  // that only the origin be sent as the referrer and it matches the previous
  // main frame URL.
  GURL referrer_url = navigation_handle->GetReferrer().url;
  if (navigation_handle->IsInMainFrame() && !referrer_url.is_empty() &&
      referrer_url == referrer_url.GetOrigin() &&
      referrer_url.GetOrigin() ==
          navigation_handle->GetPreviousMainFrameURL().GetOrigin()) {
    referrer_url = navigation_handle->GetPreviousMainFrameURL();
  }

  // Note: floc_allowed is set to false initially and is later updated by the
  // floc eligibility observer. Eventually it will be removed from the history
  // service API.
  history::HistoryAddPageArgs add_page_args(
      navigation_handle->GetURL(), timestamp,
      history::ContextIDForWebContents(web_contents()), nav_entry_id,
      referrer_url, navigation_handle->GetRedirectChain(), page_transition,
      hidden, history::SOURCE_BROWSED, navigation_handle->DidReplaceEntry(),
      ShouldConsiderForNtpMostVisited(*web_contents(), navigation_handle),
      /*floc_allowed=*/false,
      navigation_handle->IsSameDocument()
          ? absl::optional<std::u16string>(
                navigation_handle->GetWebContents()->GetTitle())
          : absl::nullopt);

  if (ui::PageTransitionIsMainFrame(page_transition) &&
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

  // Navigations in portals don't appear in history until the portal is
  // activated.
  if (navigation_handle->GetWebContents()->IsPortal())
    return;

  // No-state prefetchers should not update history. The prefetchers will have
  // their own WebContents with all observers (including |this|), and go through
  // the normal flow of a navigation, including commit.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (no_state_prefetch_manager &&
      no_state_prefetch_manager->IsWebContentsPrerendering(web_contents())) {
    return;
  }

  // Most of the time, the displayURL matches the loaded URL, but for about:
  // URLs, we use a data: URL as the real value.  We actually want to save the
  // about: URL to the history db and keep the data: URL hidden. This is what
  // the WebContents' URL getter does.
  NavigationEntry* last_committed =
      web_contents()->GetController().GetLastCommittedEntry();
  const history::HistoryAddPageArgs& add_page_args = CreateHistoryAddPageArgs(
      web_contents()->GetLastCommittedURL(), last_committed->GetTimestamp(),
      last_committed->GetUniqueID(), navigation_handle);

  if (!IsEligibleTab(add_page_args))
    return;

  UpdateHistoryForNavigation(add_page_args);

  if (HistoryClustersTabHelper* clusters_tab_helper =
          HistoryClustersTabHelper::FromWebContents(web_contents())) {
    clusters_tab_helper->OnUpdatedHistoryForNavigation(
        navigation_handle->GetNavigationId(), add_page_args.url);
  }
}

// We update history upon the associated WebContents becoming the top level
// contents of a tab from portal activation.
// TODO(mcnee): Investigate whether the early return cases in
// DidFinishNavigation apply to portal activation. See https://crbug.com/1072762
void HistoryTabHelper::DidActivatePortal(
    content::WebContents* predecessor_contents,
    base::TimeTicks activation_time) {
  history::HistoryService* hs = GetHistoryService();
  if (!hs)
    return;

  content::NavigationEntry* last_committed_entry =
      web_contents()->GetController().GetLastCommittedEntry();

  // TODO(1058504): Update this when portal activations can be done with
  // replacement.
  const bool did_replace_entry = false;

  const history::HistoryAddPageArgs add_page_args(
      last_committed_entry->GetVirtualURL(),
      last_committed_entry->GetTimestamp(),
      history::ContextIDForWebContents(web_contents()),
      last_committed_entry->GetUniqueID(),
      last_committed_entry->GetReferrer().url,
      /* redirects */ {}, ui::PAGE_TRANSITION_LINK,
      /* hidden */ false, history::SOURCE_BROWSED, did_replace_entry,
      /* consider_for_ntp_most_visited */ true,
      /* floc_allowed */ false, last_committed_entry->GetTitle());
  hs->AddPage(add_page_args);
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

bool HistoryTabHelper::IsEligibleTab(
    const history::HistoryAddPageArgs& add_page_args) const {
  if (force_eligibile_tab_for_testing_)
    return true;

#if defined(OS_ANDROID)
  auto* background_tab_manager = BackgroundTabManager::GetInstance();
  if (background_tab_manager->IsBackgroundTab(web_contents())) {
    // No history insertion is done for now since this is a tab that speculates
    // future navigations. Just caching and returning for now.
    background_tab_manager->CacheHistory(add_page_args);
    return false;
  }
  return true;
#else
  // Don't update history if this web contents isn't associated with a tab.
  return chrome::FindBrowserWithWebContents(web_contents()) != nullptr;
#endif
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryTabHelper)
