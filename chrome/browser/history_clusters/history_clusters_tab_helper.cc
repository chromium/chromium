// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"

#include <functional>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/memories_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/memories_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/web_contents.h"

#if !defined(OS_ANDROID)
#include "base/containers/contains.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/ntp_tiles/custom_links_store.h"
#else  // defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#endif  // defined(OS_ANDROID)

namespace {

using UrlAndVisitCallback =
    base::OnceCallback<void(history::URLRow, history::VisitVector)>;

// Gets the 2 most recent visits to a URL. Used to associate a memories visit
// with its history rows and compute the |duration_since_last_visit_seconds|
// context signal.
class GetMostRecentVisitsToUrl : public history::HistoryDBTask {
 public:
  GetMostRecentVisitsToUrl(const GURL& url, UrlAndVisitCallback callback)
      : url_(url), callback_(std::move(callback)) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    if (backend->GetURL(url_, &url_row_))
      backend->GetMostRecentVisitsForURL(url_row_.id(), 2, &visits_);
    return true;
  }

  void DoneRunOnMainThread() override {
    if (!visits_.empty()) {
      DCHECK_LE(visits_.size(), 2u);
      std::move(callback_).Run(url_row_, visits_);
    }
  }

 private:
  const GURL url_;
  history::URLRow url_row_;
  history::VisitVector visits_;
  UrlAndVisitCallback callback_;
};

bool IsPageInTabGroup(content::WebContents* contents) {
  DCHECK(contents);

#if !defined(OS_ANDROID)
  if (Browser* browser = chrome::FindBrowserWithWebContents(contents)) {
    int tab_index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
    if (tab_index != TabStripModel::kNoTab &&
        browser->tab_strip_model()->GetTabGroupForTab(tab_index).has_value()) {
      return true;
    }
  }
#else   // defined(OS_ANDROID)
  TabAndroid* const tab = TabAndroid::FromWebContents(contents);
  if (!tab)
    return false;
  return TabModelJniBridge::HasOtherRelatedTabs(tab);
#endif  // defined(OS_ANDROID)
  return false;
}

// Pass in a separate |url| parameter to ensure that we check the same URL that
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
  const base::TimeDelta kMaxDurationClamp = base::TimeDelta::FromDays(30);
  return delta < kMaxDurationClamp ? delta : kMaxDurationClamp;
}

}  // namespace

HistoryClustersTabHelper::HistoryClustersTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

HistoryClustersTabHelper::~HistoryClustersTabHelper() = default;

void HistoryClustersTabHelper::OnOmniboxUrlCopied() {
  // It's possible that there have been no navigations if certain builtin pages
  // were opened in a new tab (e.g. chrome://crash or chrome://invalid-page).
  if (navigation_ids_.empty())
    return;
  auto* memories_service = GetMemoriesService();
  // It's possible that the last navigation is complete if the tab crashed and a
  // new navigation hasn't began.
  if (!memories_service->HasIncompleteVisit(navigation_ids_.back()))
    return;
  memories_service->GetIncompleteVisit(navigation_ids_.back())
      .context_signals.omnibox_url_copied = true;
}

void HistoryClustersTabHelper::OnUpdatedHistoryForNavigation(
    int64_t navigation_id,
    const GURL& url) {
  StartNewNavigationIfNeeded(navigation_id);
  auto* memories_service = GetMemoriesService();
  auto& visit = memories_service->GetOrCreateIncompleteVisit(navigation_id);
  visit.context_signals.is_existing_part_of_tab_group =
      IsPageInTabGroup(web_contents());
  visit.context_signals.is_existing_bookmark =
      IsPageBookmarked(web_contents(), url);

  if (auto* history_service = GetHistoryService()) {
    // This |GetMostRecentVisitsToUrl| task will find at least 1 visit since
    // |HistoryTabHelper::UpdateHistoryForNavigation()|, invoked prior to
    // |OnUpdatedHistoryForNavigation()|, will have posted a task to add the
    // visit represented by |visit|.
    history_service->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetMostRecentVisitsToUrl>(
            url,
            base::BindOnce(
                [](HistoryClustersTabHelper* history_clusters_tab_helper,
                   memories::MemoriesService* memories_service,
                   int64_t navigation_id, memories::MemoriesVisit& visit,
                   history::URLRow url_row, history::VisitVector visits) {
                  DCHECK(history_clusters_tab_helper);
                  DCHECK(memories_service);
                  DCHECK(url_row.id());
                  DCHECK(visits[0].visit_id);
                  DCHECK_EQ(url_row.id(), visits[0].url_id);
                  visit.url_row = url_row;
                  visit.visit_row = visits[0];
                  if (visits.size() > 1) {
                    visit.context_signals.duration_since_last_visit_seconds =
                        TimeElapsedBetweenVisits(visits[1], visits[0])
                            .InSeconds();
                  }
                  // If the navigation has already ended, record the page end
                  // metrics.
                  visit.status.history_rows = true;
                  if (visit.status.navigation_ended) {
                    DCHECK(!visit.status.navigation_end_signals);
                    history_clusters_tab_helper->RecordPageEndMetricsIfNeeded(
                        navigation_id);
                  }
                },
                this, memories_service, navigation_id, std::ref(visit))),
        &task_tracker_);
  }
}

void HistoryClustersTabHelper::TagNavigationAsExpectingUkmNavigationComplete(
    int64_t navigation_id) {
  GetMemoriesService()
      ->GetOrCreateIncompleteVisit(navigation_id)
      .status.expect_ukm_page_end_signals = true;
  StartNewNavigationIfNeeded(navigation_id);
}

memories::VisitContextSignals HistoryClustersTabHelper::OnUkmNavigationComplete(
    int64_t navigation_id,
    const page_load_metrics::PageEndReason page_end_reason) {
  auto* memories_service = GetMemoriesService();
  auto& visit = memories_service->GetIncompleteVisit(navigation_id);
  visit.context_signals.page_end_reason = page_end_reason;
  // |RecordPageEndMetricsIfNeeded()| will fail to complete the visit as
  // |ukm_page_end_signals| hasn't been set yet, but it will record metrics
  // if needed (i.e. not already recorded) and possible (i.e. the history
  // request has resolved and |history_rows| have been recorded).
  RecordPageEndMetricsIfNeeded(navigation_id);
  // Make a copy of the context signals as the referenced visit may be
  // destroyed in |CompleteVisitIfReady()|.
  auto context_signals_copy = visit.context_signals;
  DCHECK(visit.status.expect_ukm_page_end_signals);
  visit.status.ukm_page_end_signals = true;
  memories_service->CompleteVisitIfReady(navigation_id);
  return context_signals_copy;
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
  auto* memories_service = GetMemoriesService();
  if (!memories_service->HasIncompleteVisit(navigation_id))
    return;
  auto& visit = memories_service->GetIncompleteVisit(navigation_id);
  if (visit.status.navigation_end_signals) {
    DCHECK(visit.status.navigation_ended);
    return;
  }
  visit.status.navigation_ended = true;
  // Don't record page end metrics if the history rows request hasn't resolved
  // because some of the metrics rely on |url_row.url()|. Setting
  // |navigation_ended| above will ensure |RecordPageEndMetricsIfNeeded()| is
  // re-invoked once the history request resolves.
  if (!visit.status.history_rows)
    return;

  visit.context_signals.is_placed_in_tab_group =
      !visit.context_signals.is_existing_part_of_tab_group &&
      IsPageInTabGroup(web_contents());
  visit.context_signals.is_new_bookmark =
      !visit.context_signals.is_existing_bookmark &&
      IsPageBookmarked(web_contents(), visit.url_row.url());
  // Android does not have NTP Custom Links.
#if !defined(OS_ANDROID)
  // This queries the prefs directly if the visit URL is stored as an NTP
  // custom link, bypassing the CustomLinksManager.
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();
  ntp_tiles::CustomLinksStore custom_link_store(pref_service);
  visit.context_signals.is_ntp_custom_link =
      base::Contains(custom_link_store.RetrieveLinks(), visit.url_row.url(),
                     [](const auto& link) { return link.url; });
#endif  // !defined(OS_ANDROID)

  visit.status.navigation_end_signals = true;
  memories_service->CompleteVisitIfReady(navigation_id);
}

void HistoryClustersTabHelper::WebContentsDestroyed() {
  for (auto navigation_id : navigation_ids_)
    RecordPageEndMetricsIfNeeded(navigation_id);
}

memories::MemoriesService* HistoryClustersTabHelper::GetMemoriesService() {
  if (!web_contents()) {
    NOTREACHED();
    return nullptr;
  }
  auto* service = MemoriesServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
  DCHECK(service);
  return service;
}

history::HistoryService* HistoryClustersTabHelper::GetHistoryService() {
  if (!web_contents()) {
    NOTREACHED();
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return HistoryServiceFactory::GetForProfileIfExists(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersTabHelper)
