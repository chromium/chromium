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

inline constexpr page_load_metrics::NavigationInitiator kBookmarkBar{
    1, "BookmarkBar"};
inline constexpr page_load_metrics::NavigationInitiator kNewTabPage{
    2, "NewTabPage"};
inline constexpr page_load_metrics::NavigationInitiator kOmniboxDirectUrlInput{
    3, "OmniboxDirectUrlInput"};
inline constexpr page_load_metrics::NavigationInitiator
    kOmniboxDefaultSearchEngine{4, "OmniboxDefaultSearchEngine"};

// This is search navigation triggered as follows:
// Android: "Web search" from the text selection context menu.
// Desktop: "Search [default search engine] for ..." from the right-click menu
// on selected text.
inline constexpr page_load_metrics::NavigationInitiator kContextMenuSearch{
    9, "ContextMenuSearch"};

// This is link navigation triggered as follows:
// Android: "Open in new tab", "Open in Incognito tab", etc. from the long
// press context menu on a link.
// Desktop: "Open link in new tab", "Open link in new window", "Open link in
// Incognito window", etc. from the right-click menu on a link.
inline constexpr page_load_metrics::NavigationInitiator kContextMenuOpenLink{
    10, "ContextMenuOpenLink"};

}  // namespace chrome_navigation_initiator
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:NavigationInitiatorType)

// The following functions attach the corresponding `NavigationInitiator` to
// `navigation_handle`. They exist so that the triggers can pass them around as
// `base::RepeatingCallback<void(content::NavigationHandle&)>`, typically as
// the `navigation_handle_callback` of `content::PageNavigator::OpenURL()`.

void AttachNewTabPageNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachOmniboxDirectUrlInputNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachOmniboxDefaultSearchEngineNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachBookmarkBarNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachContextMenuSearchNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachContextMenuOpenLinkNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

// Records that `navigation_handle` was served by DSEv1 search prefetch, which
// is out of the `content::PreloadServingMetrics` pipeline.
//
// TODO(https://crbug.com/517725655): Remove this once DSEv1 search prefetch is
// migrated to `content::PrefetchService`.
void MarkNavigationServedBySearchPrefetch(
    content::NavigationHandle& navigation_handle);

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_CHROME_NAVIGATION_INITIATOR_H_
