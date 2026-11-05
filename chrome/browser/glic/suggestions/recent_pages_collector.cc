// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/recent_pages_collector.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#endif

namespace glic {

namespace {

using Pages = std::vector<optimization_guide::proto::Tab>;

struct Candidate {
  GURL url;
  std::u16string title;
  base::Time last_active_time;
};

bool IsUrlEligibleForRecentPages(const GURL& url, Profile* profile) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (search::IsNTPOrRelatedURL(url, profile)) {
    return false;
  }
  return true;
}

#if !BUILDFLAG(IS_ANDROID)
// Appends `web_contents` to `candidates` if it was active within the lookback
// window and has an eligible URL. Filtering here keeps the walk over the tab
// strips cheap for users with many tabs: ineligible tabs cost a comparison and
// are never copied or sorted.
void AddOpenTabIfEligible(content::WebContents* web_contents,
                          base::Time min_active_time,
                          Profile* profile,
                          std::vector<Candidate>& candidates) {
  if (!web_contents || web_contents->GetLastActiveTime() < min_active_time ||
      !IsUrlEligibleForRecentPages(web_contents->GetLastCommittedURL(),
                                   profile)) {
    return;
  }
  candidates.push_back({.url = web_contents->GetLastCommittedURL(),
                        .title = web_contents->GetTitle(),
                        .last_active_time = web_contents->GetLastActiveTime()});
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::vector<Candidate> GetOpenTabCandidates(Profile* profile) {
  std::vector<Candidate> candidates;
#if !BUILDFLAG(IS_ANDROID)
  const base::Time min_active_time =
      base::Time::Now() - kMaxRecentDuration.Get();
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile ||
            browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
          return true;
        }
        TabListInterface* tab_list = TabListInterface::From(browser);
        if (!tab_list) {
          return true;
        }
        for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
          AddOpenTabIfEligible(tab ? tab->GetContents() : nullptr,
                               min_active_time, profile, candidates);
        }
        return true;
      });
#endif
  return candidates;
}

// Ranks `candidates` most recent first, drops `page_context_urls` and
// duplicate URLs (ignoring fragments), and caps at `kMaxRecentPages`. Callers
// are responsible for only passing candidates that are within the lookback
// window and have eligible URLs.
Pages SelectRecentPages(std::vector<Candidate> candidates,
                        const std::vector<GURL>& page_context_urls) {
  const size_t max_pages = std::max(0, kMaxRecentPages.Get());

  base::flat_set<GURL> excluded_urls;
  for (const GURL& url : page_context_urls) {
    excluded_urls.insert(url.GetWithoutRef());
  }

  // Most recent first, so that deduping keeps the most recent entry.
  std::ranges::stable_sort(candidates,
                           [](const Candidate& a, const Candidate& b) {
                             return a.last_active_time > b.last_active_time;
                           });

  Pages pages;
  base::flat_set<GURL> seen_urls;
  for (const Candidate& candidate : candidates) {
    if (pages.size() >= max_pages) {
      break;
    }
    GURL url_without_ref = candidate.url.GetWithoutRef();
    if (excluded_urls.contains(url_without_ref) ||
        !seen_urls.insert(url_without_ref).second) {
      continue;
    }

    optimization_guide::proto::Tab& page = pages.emplace_back();
    page.set_url(url_without_ref.spec());
    page.set_title(base::UTF16ToUTF8(candidate.title));
    page.set_last_active_timestamp_ms(
        candidate.last_active_time.InMillisecondsSinceUnixEpoch());
  }
  return pages;
}

}  // namespace

RecentPagesCollector::RecentPagesCollector(Profile* profile)
    : profile_(profile),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile,
          ServiceAccessType::EXPLICIT_ACCESS)) {
  CHECK(profile_);
  CHECK(!profile_->IsOffTheRecord());
}

RecentPagesCollector::~RecentPagesCollector() = default;

void RecentPagesCollector::CollectRecentPages(
    const std::vector<GURL>& page_context_urls,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (kRecentPagesSource.Get() == RecentPagesSource::kOpenTabsOnly) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RecentPagesCollector::CollectFromOpenTabs,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  page_context_urls, std::move(callback)));
    return;
  }

  // Return nothing rather than block on the backend loading, so the caller
  // never waits for history. Also return nothing for a non-positive cap, since
  // a `max_count` of 0 would make the query unbounded.
  const int max_pages = kMaxRecentPages.Get();
  if (max_pages <= 0 || !history_service_ ||
      !history_service_->BackendLoaded()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), Pages()));
    return;
  }

  history::QueryOptions options;
  options.begin_time = base::Time::Now() - kMaxRecentDuration.Get();
  // Leave headroom for rows dropped by the filters below, e.g. #fragment
  // variants of the same page, which history does not deduplicate.
  options.max_count = max_pages * 10;
  options.policy_for_404_visits = history::VisitQuery404sPolicy::kExclude404s;
  // Only include visits originating from the local device (stored with an
  // empty `originator_cache_guid`) that are known to sync.
  options.restrict_to_synced_urls = true;
  options.client_ids = {""};
  history_service_->QueryHistory(
      /*text_query=*/std::u16string(), options,
      base::BindOnce(&RecentPagesCollector::OnHistoryQueryDone,
                     weak_ptr_factory_.GetWeakPtr(), page_context_urls,
                     std::move(callback)),
      &task_tracker_);
}

void RecentPagesCollector::OnHistoryQueryDone(
    const std::vector<GURL>& page_context_urls,
    Callback callback,
    history::QueryResults results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Recency is already enforced by the query's `begin_time`.
  std::vector<Candidate> candidates;
  candidates.reserve(results.size());
  for (const history::URLResult& result : results) {
    if (result.blocked_visit() ||
        !IsUrlEligibleForRecentPages(result.url(), profile_)) {
      continue;
    }
    candidates.push_back({.url = result.url(),
                          .title = result.title(),
                          .last_active_time = result.visit_time()});
  }
  std::move(callback).Run(
      SelectRecentPages(std::move(candidates), page_context_urls));
}

void RecentPagesCollector::CollectFromOpenTabs(
    const std::vector<GURL>& page_context_urls,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(
      SelectRecentPages(GetOpenTabCandidates(profile_), page_context_urls));
}

}  // namespace glic
