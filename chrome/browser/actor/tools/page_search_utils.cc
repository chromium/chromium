// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_search_utils.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/i18n/string_search.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace actor {

namespace {

// Max candidates to fetch from bookmarks/history so we can skip disallowed
// URLs and still find valid matches without scanning everything.
constexpr size_t kMaxSearchMatches = 10;

#if !BUILDFLAG(IS_ANDROID)
// Case- and accent-insensitive substring test. Page titles are routinely
// non-ASCII, so ASCII-only lowercasing would fail to match them (e.g. a tab
// titled "ÄPFEL" would not match the query "äpfel").
bool ContainsCaseInsensitive(std::u16string_view haystack,
                             const std::u16string& needle) {
  return base::i18n::StringSearchIgnoringCaseAndAccents(
      needle, haystack, /*match_index=*/nullptr, /*match_length=*/nullptr);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void OnHistoryQueryFinished(PageMatchesCallback callback,
                            UrlMatchFilter filter,
                            history::QueryResults results) {
  std::vector<PageMatch> matches;
  for (const history::URLResult& result : results) {
    if (IsAllowedMatchUrl(result.url(), filter)) {
      matches.emplace_back(result.url(), result.title());
    }
  }
  std::move(callback).Run(std::move(matches));
}

}  // namespace

bool IsAllowedMatchUrl(const GURL& url, UrlMatchFilter filter) {
  if (!url.is_valid()) {
    return false;
  }
  if (url.SchemeIsHTTPOrHTTPS()) {
    return filter.Has(UrlMatchCategory::kHttpOrHttps);
  }
  if (!filter.Has(UrlMatchCategory::kNewTabPage)) {
    return false;
  }
  // The NTP is reachable under several hosts depending on which
  // implementation is active.
  static constexpr auto kNewTabPageHosts = std::to_array<std::string_view>(
      {chrome::kChromeUINewTabHost, chrome::kChromeUINewTabPageHost,
       chrome::kChromeUINewTabPageThirdPartyHost});
  return url.SchemeIs(content::kChromeUIScheme) &&
         std::ranges::contains(kNewTabPageHosts, url.host());
}

#if !BUILDFLAG(IS_ANDROID)
std::vector<TabMatch> FindMatchingTabs(BrowserWindowInterface* browser,
                                       std::string_view query,
                                       UrlMatchFilter filter) {
  if (query.empty() || !browser) {
    return {};
  }

  TabStripModel* tab_strip_model = browser->GetTabStripModel();
  if (!tab_strip_model) {
    return {};
  }

  const std::u16string query16 = base::UTF8ToUTF16(query);
  std::vector<TabMatch> matched_tabs;

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(i);
    if (!tab) {
      continue;
    }
    content::WebContents* contents = tab->GetContents();
    if (!contents) {
      continue;
    }

    const GURL& committed_url = contents->GetLastCommittedURL();
    if (!IsAllowedMatchUrl(committed_url, filter)) {
      continue;
    }

    const std::u16string title = contents->GetTitle();
    if (ContainsCaseInsensitive(title, query16) ||
        ContainsCaseInsensitive(base::UTF8ToUTF16(committed_url.spec()),
                                query16)) {
      matched_tabs.emplace_back(tab->GetHandle(), committed_url, title);
    }
  }

  return matched_tabs;
}

tabs::TabInterface* ResolveTabInWindow(const TabMatch& match,
                                       BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  tabs::TabInterface* tab = match.handle.Get();
  if (!tab) {
    // The tab was closed after the search ran.
    return nullptr;
  }
  if (tab->GetBrowserWindowInterface() != browser) {
    // The tab was moved to another window after the search ran. Acting on it
    // would pull focus to a window the caller never searched.
    return nullptr;
  }
  return tab;
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::vector<PageMatch> FindMatchingBookmarks(Profile* profile,
                                             std::string_view query,
                                             UrlMatchFilter filter) {
  if (query.empty() || !profile) {
    return {};
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!bookmark_model || !bookmark_model->loaded()) {
    return {};
  }

  bookmarks::QueryFields query_fields;
  query_fields.word_phrase_query =
      std::make_unique<std::u16string>(base::UTF8ToUTF16(query));
  // The returned nodes are owned by `bookmark_model`; they are only used
  // synchronously below to copy out the fields we need.
  std::vector<const bookmarks::BookmarkNode*> candidates =
      bookmarks::GetBookmarksMatchingProperties(bookmark_model, query_fields,
                                                kMaxSearchMatches);

  std::vector<PageMatch> matches;
  for (const bookmarks::BookmarkNode* node : candidates) {
    if (node->is_url() && IsAllowedMatchUrl(node->url(), filter)) {
      matches.emplace_back(node->url(), node->GetTitle());
    }
  }

  return matches;
}

void FindMatchingHistory(Profile* profile,
                         std::string_view query,
                         base::CancelableTaskTracker* tracker,
                         UrlMatchFilter filter,
                         PageMatchesCallback callback) {
  if (query.empty() || !profile || !tracker) {
    std::move(callback).Run({});
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    std::move(callback).Run({});
    return;
  }

  history::QueryOptions options;
  options.max_count = static_cast<int>(kMaxSearchMatches);
  options.duplicate_policy = history::QueryOptions::REMOVE_ALL_DUPLICATES;

  history_service->QueryHistory(
      base::UTF8ToUTF16(query), options,
      base::BindOnce(&OnHistoryQueryFinished, std::move(callback), filter),
      tracker);
}

void FindMatchingHistory(Profile* profile,
                         std::string_view query,
                         base::CancelableTaskTracker* tracker,
                         PageMatchesCallback callback) {
  FindMatchingHistory(profile, query, tracker, UrlMatchFilter::All(),
                      std::move(callback));
}

}  // namespace actor
