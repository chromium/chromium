// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/memories_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/memories/core/memories_service.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/navigation_handle.h"

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

}  // namespace

HistoryClustersTabHelper::HistoryClustersTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

HistoryClustersTabHelper::~HistoryClustersTabHelper() = default;

void HistoryClustersTabHelper::LogUrlCopied() {
  if (!visits_.empty()) {
    visits_.back().context_signals.omnibox_url_copied = true;
  }
}

void HistoryClustersTabHelper::DidUpdateHistoryForNavigation(
    content::NavigationHandle* navigation_handle,
    const history::HistoryAddPageArgs& add_page_args) {
  if (!visits_.empty()) {
    // We are starting a new navigation, so record the page end metrics.
    // TODO(tommycli): We don't know the page end reason, but maybe we could
    // guess and assume it's a same document navigation. Investigate this.
    RecordPageEndMetricsIfNeeded(visits_.back());
  }

  // Copy over exactly what's stored in History for the URL and timestamp.
  //
  // TODO(tommycli): HistoryAddPageArgs is further manipulated in the
  // HistoryBackend where we cannot access it. We may need to have a more
  // advanced approach to make sure we are accurately reflecting History.
  auto visit = memories::MemoriesVisit(navigation_handle->GetNavigationId(),
                                       add_page_args.url);
  visit.visit_time = add_page_args.time;

  // TODO(tommycli): add_page_args also contains the context_id and nav_entry_id
  // that we probably need to store to get the database visit_id. Maybe we
  // should fetch the visit_id in this location... or maybe save the context_id
  // and nav_entry_id for saving before we flush to MemoriesService.

  // Record page-start context signals.
  visit.context_signals.is_existing_part_of_tab_group =
      IsPageInTabGroup(web_contents());
  visit.context_signals.is_existing_bookmark =
      IsPageBookmarked(web_contents(), visit.url);
  visits_.push_back(visit);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileIfExists(
          profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service) {
    // Using |visit.visit_time| as the |end_time| param will exactly exclude the
    // current visit, as the range is exclusive at the end. Using
    // base::Unretained is fine here, as |task_tracker_| prevents
    // use-after-free.
    //
    // TODO(tommycli): Perhaps we should migrate this call to MemoriesService,
    // in case the user closes the tab too quickly after we make this call.
    history_service->GetLastVisitToURL(
        visit.url, visit.visit_time,
        base::BindOnce(&HistoryClustersTabHelper::PreviousVisitToUrlCallback,
                       base::Unretained(this),
                       navigation_handle->GetNavigationId()),
        &task_tracker_);
  }
}

base::Optional<memories::MemoriesVisit>
HistoryClustersTabHelper::UpdatePageEndReasonAndGetVisitForUkm(
    int64_t navigation_id,
    const page_load_metrics::PageEndReason page_end_reason) {
  // Find the visit that matches the navigation id.
  auto visit_it = base::ranges::find(visits_, navigation_id,
                                     [](auto& v) { return v.navigation_id; });
  if (visit_it == visits_.end())
    return base::nullopt;

  visit_it->context_signals.page_end_reason = page_end_reason;
  RecordPageEndMetricsIfNeeded(*visit_it);

  // Return a copy to the caller, as we are about to flush away our copy.
  base::Optional<memories::MemoriesVisit> copy = *visit_it;

  // We are done with this visit. Send it to the service, if available, then
  // delete from our own vector.
  if (memories::MemoriesService* service =
          MemoriesServiceFactory::GetForBrowserContext(
              web_contents()->GetBrowserContext())) {
    service->AddVisit(*visit_it);
    visits_.erase(visit_it);
  }
  return copy;
}

void HistoryClustersTabHelper::RecordPageEndMetricsIfNeeded(
    memories::MemoriesVisit& visit) {
  if (visit.page_end_metrics_recorded)
    return;

  visit.page_end_metrics_recorded = true;

  visit.context_signals.is_placed_in_tab_group =
      !visit.context_signals.is_existing_part_of_tab_group &&
      IsPageInTabGroup(web_contents());
  visit.context_signals.is_new_bookmark =
      !visit.context_signals.is_existing_bookmark &&
      IsPageBookmarked(web_contents(), visit.url);

  // Android does not have NTP Custom Links.
#if !defined(OS_ANDROID)
  // This queries the prefs directly if the visit URL is stored as an NTP
  // custom link, bypassing the CustomLinksManager.
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();
  ntp_tiles::CustomLinksStore custom_link_store(pref_service);
  visit.context_signals.is_ntp_custom_link =
      base::Contains(custom_link_store.RetrieveLinks(), visit.url,
                     [](const auto& link) { return link.url; });
#endif  // !defined(OS_ANDROID)
}

void HistoryClustersTabHelper::WebContentsDestroyed() {
  memories::MemoriesService* service =
      MemoriesServiceFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!service)
    return;

  // Finalize and flush any leftover visits, which may be same-doc navigations.
  // TODO(tommycli): We really should flush these faster than
  // WebContentsDestroyed(). We should also make a guess of the page end reason.
  // TODO(tommycli): When a tab is closed, UKM updates the page end reason AFTER
  // the WebContents is destroyed. For those last pages, the page end reason is
  // not properly recorded, nor is UKM recorded properly. Fix this.
  for (auto& visit : visits_) {
    RecordPageEndMetricsIfNeeded(visit);
    service->AddVisit(visit);
  }
  visits_.clear();
}

void HistoryClustersTabHelper::PreviousVisitToUrlCallback(
    int64_t navigation_id,
    history::HistoryLastVisitResult result) {
  if (!result.success || result.last_visit.is_null())
    return;

  // Find the visit that matches the navigation id.
  auto visit_it = base::ranges::find(visits_, navigation_id,
                                     [](auto& v) { return v.navigation_id; });
  if (visit_it == visits_.end())
    return;

  base::TimeDelta since_last_visit = visit_it->visit_time - result.last_visit;

  // Clamp to 30 days maximum to match the UKM retention period.
  const base::TimeDelta kMaxDurationClamp = base::TimeDelta::FromDays(30);
  if (since_last_visit > kMaxDurationClamp) {
    since_last_visit = kMaxDurationClamp;
  }

  visit_it->context_signals.duration_since_last_visit_seconds =
      since_last_visit.InSeconds();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersTabHelper)
