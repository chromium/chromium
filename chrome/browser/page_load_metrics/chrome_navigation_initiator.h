// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_CHROME_NAVIGATION_INITIATOR_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_CHROME_NAVIGATION_INITIATOR_H_

#include "components/page_load_metrics/browser/navigation_initiator.h"

// `page_load_metrics::NavigationInitiator`s that are specific to //chrome.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Both the value and the name must be
// unique across all the layers; use 200-255 for new ones.
//
// Note that an initiator that `ui::PageTransition` can tell (e.g. a link click
// or a reload) must not be defined here. See
// `page_load_metrics::NavigationInitiator` for more details.
//
// LINT.IfChange(ChromeNavigationInitiator)
namespace chrome_navigation_initiator {

// Navigation triggered from the bookmark bar.
//
// Attached when opening bookmarks by clicking a top-level bookmark button on
// the bookmark bar, or via the bookmark bar page handler. Note that bookmarks
// inside folders are not supported yet.
inline constexpr page_load_metrics::NavigationInitiator kBookmarkBar{
    1, "BookmarkBar"};

// Navigation triggered from the New Tab Page (NTP).
//
// Attached when clicking a Most Visited tile or shortcut on the NTP.
inline constexpr page_load_metrics::NavigationInitiator kNewTabPage{
    2, "NewTabPage"};

// Navigation triggered from the omnibox by typing a URL directly.
//
// Attached by `ChromeOmniboxClient` (Desktop) or
// `AutocompleteControllerAndroid` (Android) when the user enters a URL directly
// (`ui::PAGE_TRANSITION_TYPED`).
inline constexpr page_load_metrics::NavigationInitiator kOmniboxDirectUrlInput{
    3, "OmniboxDirectUrlInput"};

// Navigation triggered from the omnibox by submitting a search query.
//
// Attached by `ChromeOmniboxClient` (Desktop) or
// `AutocompleteControllerAndroid` (Android) when the user searches with the
// default search engine
// (`ui::PAGE_TRANSITION_GENERATED`).
inline constexpr page_load_metrics::NavigationInitiator
    kOmniboxDefaultSearchEngine{4, "OmniboxDefaultSearchEngine"};

// Navigation triggered by searching from the context menu.
//
// - Android: "Web search" from the text selection context menu.
// - Desktop: "Search [default search engine] for ..." from the right-click menu
//   on selected text.
inline constexpr page_load_metrics::NavigationInitiator kContextMenuSearch{
    9, "ContextMenuSearch"};

// Navigation triggered by opening a link from the context menu.
//
// - Android: "Open in new tab", "Open in Incognito tab", etc. from the long
//   press context menu on a link.
// - Desktop: "Open link in new tab", "Open link in new window", "Open link in
//   Incognito window", etc. from the right-click menu on a link.
inline constexpr page_load_metrics::NavigationInitiator kContextMenuOpenLink{
    10, "ContextMenuOpenLink"};

}  // namespace chrome_navigation_initiator
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:NavigationInitiatorType)

// Records that `navigation_handle` was served by DSEv1 search prefetch, which
// is out of the `content::PreloadServingMetrics` pipeline.
//
// TODO(https://crbug.com/517725655): Remove this once DSEv1 search prefetch is
// migrated to `content::PrefetchService`.
void MarkNavigationServedBySearchPrefetch(
    content::NavigationHandle& navigation_handle);

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_CHROME_NAVIGATION_INITIATOR_H_
