// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_SEARCH_UTILS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_SEARCH_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
// TabMatch stores a tabs::TabHandle by value. TabHandle is an alias for the
// nested TabInterface::Handle, so the full definition is required here.
#include "components/tabs/public/tab_interface.h"
#endif  // !BUILDFLAG(IS_ANDROID)

class Profile;

#if !BUILDFLAG(IS_ANDROID)
class BrowserWindowInterface;
#endif  // !BUILDFLAG(IS_ANDROID)

namespace base {
class CancelableTaskTracker;
}  // namespace base

namespace actor {

// Categories of URL a page search is permitted to match. Anything not covered
// by an enabled category is excluded, so unsafe or internal schemes
// (file://, javascript:, chrome://settings, devtools://, ...) are rejected by
// construction rather than by blocklist.
enum class UrlMatchCategory {
  // Ordinary web pages.
  kHttpOrHttps,
  // The New Tab Page, under any of its host aliases.
  kNewTabPage,

  kMinValue = kHttpOrHttps,
  kMaxValue = kNewTabPage,
};

// Set by the caller at tool creation. Defaults to every category; callers
// needing a narrower surface can pass a subset.
using UrlMatchFilter = base::EnumSet<UrlMatchCategory,
                                     UrlMatchCategory::kMinValue,
                                     UrlMatchCategory::kMaxValue>;

// A page that matched a search query. Fields are copied, so results remain
// valid after the underlying model (tab strip, BookmarkModel, HistoryService)
// changes.
struct PageMatch {
  GURL url;
  std::u16string title;
};

#if !BUILDFLAG(IS_ANDROID)
// A matching open tab. `handle` is safe to persist; resolve it with
// tabs::TabHandle::Get() immediately before use, and null-check the result.
struct TabMatch {
  tabs::TabHandle handle;
  GURL url;
  std::u16string title;
};
#endif  // !BUILDFLAG(IS_ANDROID)

// Returns true if `url` is a valid navigation target for page search matches
// under `filter`.
bool IsAllowedMatchUrl(const GURL& url,
                       UrlMatchFilter filter = UrlMatchFilter::All());

#if !BUILDFLAG(IS_ANDROID)
// Searches open tabs in `browser` in tab-strip order.
// Returns all tabs with an allowed committed URL (see `IsAllowedMatchUrl`)
// whose title or URL contains `query` (case-insensitive substring match).
// Returns an empty vector if `browser` is null, `query` is empty, or no
// eligible tab matches.
//
// SINGLE WINDOW ONLY. Only tabs in `browser` are ever considered; tabs in
// other windows are never returned, even when they match. This is intended
// behavior, not a limitation: a tab switch acts on the window the user is
// looking at and must not pull focus to a different window.
std::vector<TabMatch> FindMatchingTabs(
    BrowserWindowInterface* browser,
    std::string_view query,
    UrlMatchFilter filter = UrlMatchFilter::All());

// Resolves `match` back to a live tab, or returns nullptr if the tab has been
// closed *or* has moved to a window other than `browser` since the search ran.
//
// Callers must use this instead of calling tabs::TabHandle::Get() directly.
// A tab can be dragged into another window between the search and the action
// that uses its result; acting on the bare handle would then activate a tab in
// a different window, silently violating the single-window guarantee above.
tabs::TabInterface* ResolveTabInWindow(const TabMatch& match,
                                       BrowserWindowInterface* browser);
#endif  // !BUILDFLAG(IS_ANDROID)

// Searches bookmarks for `profile` using BookmarkModel.
// Returns all matching bookmarks with an allowed URL whose title or URL
// matches `query`.
// Returns an empty vector if `query` is empty, BookmarkModel is not loaded, or
// no eligible match is found.
std::vector<PageMatch> FindMatchingBookmarks(
    Profile* profile,
    std::string_view query,
    UrlMatchFilter filter = UrlMatchFilter::All());

// Searches history for `profile` asynchronously via HistoryService.
// Calls `callback` with the list of matching history results.
// If `query` is empty, `profile` or `tracker` is null, or HistoryService is
// unavailable, runs `callback` synchronously with an empty vector.
using PageMatchesCallback = base::OnceCallback<void(std::vector<PageMatch>)>;
void FindMatchingHistory(Profile* profile,
                         std::string_view query,
                         base::CancelableTaskTracker* tracker,
                         UrlMatchFilter filter,
                         PageMatchesCallback callback);

// Same as above, matching every URL category.
void FindMatchingHistory(Profile* profile,
                         std::string_view query,
                         base::CancelableTaskTracker* tracker,
                         PageMatchesCallback callback);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_SEARCH_UTILS_H_
