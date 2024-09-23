// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "base/containers/contains.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/ntp_tiles/custom_links_store.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

bool IsPageInTabGroup(content::WebContents* contents) {
  DCHECK(contents);

#if !BUILDFLAG(IS_ANDROID)
  if (Browser* browser = chrome::FindBrowserWithTab(contents)) {
    int tab_index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
    if (tab_index != TabStripModel::kNoTab &&
        browser->tab_strip_model()->GetTabGroupForTab(tab_index).has_value()) {
      return true;
    }
  }
  return false;
#else   // BUILDFLAG(IS_ANDROID)
  TabAndroid* const tab = TabAndroid::FromWebContents(contents);
  if (!tab)
    return false;
  return TabModelJniBridge::IsTabInTabGroup(tab);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Pass in a separate `url` parameter to ensure that we check the same URL that
// is being logged in History.
bool IsPageBookmarked(content::WebContents* contents, const GURL& url) {
  DCHECK(contents);
  DCHECK(contents->GetBrowserContext());

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(contents->GetBrowserContext());
  return model && model->IsBookmarked(url);
}

base::TimeDelta TimeElapsedBetweenVisits(const history::VisitRow& visit1,
                                         const history::VisitRow& visit2) {
  base::TimeDelta delta = visit2.visit_time - visit1.visit_time;
  // Clamp to 30 days maximum to match the UKM retention period.
  const base::TimeDelta kMaxDurationClamp = base::Days(30);
  return delta < kMaxDurationClamp ? delta : kMaxDurationClamp;
}

// Returns with the provided `url` matches the provided `history_url`
// which must be either the basic history URL or history_clusters URL.
bool IsHistoryPage(GURL url, const GURL& history_url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements) == history_url;
}

}  // namespace

HistoryClustersTabHelper::HistoryClustersTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HistoryClustersTabHelper>(*web_contents) {}

HistoryClustersTabHelper::~HistoryClustersTabHelper() = default;

void HistoryClustersTabHelper::OnOmniboxUrlCopied() {
  // It's possible that there have been no navigations if certain builtin pages
  // were opened in a new tab (e.g. chrome://crash or chrome://invalid-page).
  if (navigation_ids_.empty())
    return;

  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  // It's possible that the last navigation is complete if the tab crashed and a
  // new navigation hasn't began.
  if (!history_clusters_service->HasIncompleteVisitContextAnnotations(
          navigation_ids_.back())) {
    return;
  }

  history_clusters_service
      ->GetIncompleteVisitContextAnnotations(navigation_ids_.back())
      .context_annotations.omnibox_url_copied = true;
}

void HistoryClustersTabHelper::OnOmniboxUrlShared() {
  // TODO(crbug.com/40166126): possibly update a different context annotation.
  OnOmniboxUrlCopied();
}

void HistoryClustersTabHelper::OnUpdatedHistoryForNavigation(
    int64_t navigation_id,
    base::Time timestamp,
    const GURL& url) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  StartNewNavigationIfNeeded(navigation_id);

  auto& incomplete_visit_context_annotations =
      history_clusters_service->GetOrCreateIncompleteVisitContextAnnotations(
          navigation_id);
  incomplete_visit_context_annotations.context_annotations
      .is_existing_part_of_tab_group = IsPageInTabGroup(web_contents());
  incomplete_visit_context_annotations.context_annotations
      .is_existing_bookmark = IsPageBookmarked(web_contents(), url);

  if (auto* history_service = GetHistoryService()) {
    // This `GetMostRecentVisitsToUrl` task should find at least 1 visit since
    // `HistoryTabHelper::UpdateHistoryForNavigation()`, invoked prior to
    // `OnUpdatedHistoryForNavigation()`, will have posted a task to add the
    // visit associated to `incomplete_visit_context_annotations`.
    history_service->GetMostRecentVisitsForGurl(
        url, 2,
        base::BindOnce(
            [](HistoryClustersTabHelper* history_clusters_tab_helper,
               history_clusters::HistoryClustersService*
                   history_clusters_service,
               int64_t navigation_id, base::Time timestamp,
               history_clusters::IncompleteVisitContextAnnotations&
                   incomplete_visit_context_annotations,
               history::QueryURLResult result) {
              DCHECK(history_clusters_tab_helper);
              DCHECK(history_clusters_service);
              // visit being added to the DB, e.g. navigations to
              // "chrome://" URLs.
              if (!result.success || result.visits.empty()) {
                return;
              }
              const history::URLRow& url_row = result.row;
              const history::VisitVector& visits = result.visits;
              DCHECK(url_row.id());
              DCHECK(visits[0].visit_id);
              DCHECK_EQ(url_row.id(), visits[0].url_id);
              // Make sure the visit we got actually corresponds to the
              // navigation by comparing the timestamps.
              if (visits[0].visit_time != timestamp) {
                return;
              }
              // Make sure the latest visit (the first one in the array) is
              // a local one. That should almost always be the case, since
              // this gets called just after a local visit happened, but in
              // some rare cases it might not be, e.g. if another device
              // sent us a visit "from the future". If this turns out to be
              // a problem, consider implementing a
              // GetMostRecent*Local*VisitsForURL().
              if (!visits[0].originator_cache_guid.empty()) {
                return;
              }
              incomplete_visit_context_annotations.url_row = url_row;
              incomplete_visit_context_annotations.visit_row = visits[0];
              if (visits.size() > 1) {
                incomplete_visit_context_annotations.context_annotations
                    .duration_since_last_visit =
                    TimeElapsedBetweenVisits(visits[1], visits[0]);
              }
              // If the navigation has already ended, record the page end
              // metrics.
              incomplete_visit_context_annotations.status.history_rows = true;
              if (incomplete_visit_context_annotations.status
                      .navigation_ended) {
                DCHECK(!incomplete_visit_context_annotations.status
                            .navigation_end_signals);
                history_clusters_tab_helper->RecordPageEndMetricsIfNeeded(
                    navigation_id);
              }
            },
            this, history_clusters_service, navigation_id, timestamp,
            std::ref(incomplete_visit_context_annotations)),
        &task_tracker_);
  }
}

void HistoryClustersTabHelper::TagNavigationAsExpectingUkmNavigationComplete(
    int64_t navigation_id) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  history_clusters_service
      ->GetOrCreateIncompleteVisitContextAnnotations(navigation_id)
      .status.expect_ukm_page_end_signals = true;
  StartNewNavigationIfNeeded(navigation_id);
}

history::VisitContextAnnotations
HistoryClustersTabHelper::OnUkmNavigationComplete(
    int64_t navigation_id,
    base::TimeDelta total_foreground_duration,
    const page_load_metrics::PageEndReason page_end_reason) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return history::VisitContextAnnotations();

  auto& incomplete_visit_context_annotations =
      history_clusters_service->GetIncompleteVisitContextAnnotations(
          navigation_id);
  incomplete_visit_context_annotations.context_annotations.page_end_reason =
      page_end_reason;
  incomplete_visit_context_annotations.context_annotations
      .total_foreground_duration = total_foreground_duration;
  // `RecordPageEndMetricsIfNeeded()` will fail to complete the
  // `IncompleteVisitContextAnnotations` as `ukm_page_end_signals` hasn't been
  // set yet, but it will record metrics if needed (i.e. not already recorded)
  // and possible (i.e. the history request has resolved and `history_rows` have
  // been recorded).
  RecordPageEndMetricsIfNeeded(navigation_id);
  // Make a copy of the context annotations as the referenced
  // incomplete_visit_context_annotations may be destroyed in
  // `CompleteVisitContextAnnotationsIfReady()`.
  auto context_annotations_copy =
      incomplete_visit_context_annotations.context_annotations;
  DCHECK(
      incomplete_visit_context_annotations.status.expect_ukm_page_end_signals);
  incomplete_visit_context_annotations.status.ukm_page_end_signals = true;
  history_clusters_service->CompleteVisitContextAnnotationsIfReady(
      navigation_id);
  return context_annotations_copy;
}

void HistoryClustersTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Will detect when the history clusters page was toggled to the basic history
  // page.

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // The remaining logic only pertains to if the previously committed navigation
  // was the HistoryClusters UI.
  if (!IsHistoryPage(navigation_handle->GetWebContents()->GetLastCommittedURL(),
                     GURL(history_clusters::GetChromeUIHistoryClustersURL()))) {
    return;
  }

  // Detect toggling to another history UI:
  // 1) Previous committed navigation was the HistoryClusters UI.
  // 2) This is a same doc navigation.
  if (navigation_handle->IsSameDocument()) {
    auto* logger =
        history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
            navigation_handle->GetWebContents()->GetPrimaryPage());
    // When the user navigates away from the HistoryClusters UI to the
    // ChromeHistory UI, increment the toggles count.
    if (IsHistoryPage(navigation_handle->GetURL(),
                      GURL(chrome::kChromeUIHistoryURL))) {
      logger->increment_toggles_to_basic_history();
    }
  }
}

void HistoryClustersTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Will detect when the history clusters page was navigated to, either
  // directly (e.g., through the omnibox, by page refresh, or by page
  // back/forward), or indirectly (e.g. through the side bar on the history
  // page). And will update this page's associated
  // `HistoryClustersMetricsLogger`.

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (!IsHistoryPage(navigation_handle->GetURL(),
                     GURL(history_clusters::GetChromeUIHistoryClustersURL()))) {
    return;
  }

  // Loggers should only record the initial state once. This code will be called
  // again if the user subsequently switches in-page tabs between History and
  // Journeys, or if the user makes an in-page search. We should early return in
  // those cases because we want to preserve the state of the INITIAL arrival.
  auto* logger =
      history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
          navigation_handle->GetWebContents()->GetPrimaryPage());
  if (logger->initial_state()) {
    return;
  }
  // The WebContentsObserver::OnVisibilityChanged() doesn't fire on navigation,
  // so we need to manually notify the logger about the visibility state when
  // navigating to the history clusters page from another visible page.
  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE) {
    logger->WasShown();
  }

  logger->set_navigation_id(navigation_handle->GetNavigationId());

  // Indirect navigation is kind of our catch-all, although in practice it
  // pretty much means the omnibox action chip.
  auto initial_state =
      history_clusters::HistoryClustersInitialState::kIndirectNavigation;

  if (navigation_handle->IsSameDocument()) {
    initial_state =
        history_clusters::HistoryClustersInitialState::kSameDocument;
  } else if (PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                      ui::PAGE_TRANSITION_TYPED) ||
             PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                      ui::PAGE_TRANSITION_RELOAD)) {
    // If the transition type is typed (meaning directly entered into the
    // address bar), PAGE_TRANSITION_TYPED, or is partially typed and selected
    // from the omnibox history, which results in PAGE_TRANSITION_RELOADS, this
    // usage of the history clusters UI is considered a "direct" navigation.
    initial_state =
        history_clusters::HistoryClustersInitialState::kDirectNavigation;
  }

  logger->set_initial_state(initial_state);
}

void HistoryClustersTabHelper::WebContentsDestroyed() {
  // Complete any incomplete visits associated with navigations made in this
  // tab.
  for (auto navigation_id : navigation_ids_)
    RecordPageEndMetricsIfNeeded(navigation_id);
}

void HistoryClustersTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    return;
  }
  history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
      web_contents()->GetPrimaryPage())
      ->WasShown();
}

void HistoryClustersTabHelper::StartNewNavigationIfNeeded(
    int64_t navigation_id) {
  if (!navigation_ids_.empty() && navigation_id == navigation_ids_.back())
    return;

  // We are starting a new navigation, so record the navigation end metrics for
  // the previous navigation.
  // TODO(tommycli): We don't know the page end reason, but maybe we could guess
  //  and assume it's a same document navigation. Investigate this.
  if (!navigation_ids_.empty())
    RecordPageEndMetricsIfNeeded(navigation_ids_.back());
  navigation_ids_.push_back(navigation_id);
}

void HistoryClustersTabHelper::RecordPageEndMetricsIfNeeded(
    int64_t navigation_id) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  if (!history_clusters_service->HasIncompleteVisitContextAnnotations(
          navigation_id))
    return;
  auto& incomplete_visit_context_annotations =
      history_clusters_service->GetIncompleteVisitContextAnnotations(
          navigation_id);
  if (incomplete_visit_context_annotations.status.navigation_end_signals) {
    DCHECK(incomplete_visit_context_annotations.status.navigation_ended);
    return;
  }
  incomplete_visit_context_annotations.status.navigation_ended = true;
  // Don't record page end metrics if the history rows request hasn't resolved
  // because some of the metrics rely on |url_row.url()|. Setting
  // `navigation_ended` above will ensure `RecordPageEndMetricsIfNeeded()` is
  // re-invoked once the history request resolves.
  if (!incomplete_visit_context_annotations.status.history_rows)
    return;

  incomplete_visit_context_annotations.context_annotations
      .is_placed_in_tab_group =
      !incomplete_visit_context_annotations.context_annotations
           .is_existing_part_of_tab_group &&
      IsPageInTabGroup(web_contents());
  incomplete_visit_context_annotations.context_annotations.is_new_bookmark =
      !incomplete_visit_context_annotations.context_annotations
           .is_existing_bookmark &&
      IsPageBookmarked(web_contents(),
                       incomplete_visit_context_annotations.url_row.url());
  // Android does not have NTP Custom Links.
#if !BUILDFLAG(IS_ANDROID)
  // This queries the prefs directly if the visit URL is stored as an NTP
  // custom link, bypassing the CustomLinksManager.
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();
  ntp_tiles::CustomLinksStore custom_link_store(pref_service);
  incomplete_visit_context_annotations.context_annotations.is_ntp_custom_link =
      base::Contains(custom_link_store.RetrieveLinks(),
                     incomplete_visit_context_annotations.url_row.url(),
                     [](const auto& link) { return link.url; });
#endif  // !BUILDFLAG(IS_ANDROID)

  incomplete_visit_context_annotations.status.navigation_end_signals = true;
  history_clusters_service->CompleteVisitContextAnnotationsIfReady(
      navigation_id);
}

history_clusters::HistoryClustersService*
HistoryClustersTabHelper::GetHistoryClustersService() {
  if (!web_contents()) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  return HistoryClustersServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

history::HistoryService* HistoryClustersTabHelper::GetHistoryService() {
  if (!web_contents()) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return HistoryServiceFactory::GetForProfileIfExists(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersTabHelper);
